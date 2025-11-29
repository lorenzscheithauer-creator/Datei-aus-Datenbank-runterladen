#include <array>
#include <chrono>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct Config {
    fs::path link_file;
    fs::path prompt_file;
    fs::path progress_file;
    fs::path result_file;
    std::string model_name = "deepseek-r1:7b";
    unsigned int interval_seconds = 900; // 15 minutes
};

struct PromptResult {
    std::string prompt;
    std::string response;
};

// ---------- Utility helpers ----------

std::string trim(const std::string &input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

std::string escape_json(const std::string &input) {
    std::ostringstream escaped;
    for (char c : input) {
        switch (c) {
        case '\\': escaped << "\\\\"; break;
        case '"': escaped << "\\\""; break;
        case '\n': escaped << "\\n"; break;
        case '\r': escaped << "\\r"; break;
        case '\t': escaped << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                escaped << "\\u";
                escaped << std::hex;
                escaped.width(4);
                escaped.fill('0');
                escaped << static_cast<int>(static_cast<unsigned char>(c));
            } else {
                escaped << c;
            }
        }
    }
    return escaped.str();
}

std::string escape_shell_single_quotes(const std::string &input) {
    std::ostringstream out;
    for (char c : input) {
        if (c == '\'') {
            out << "'\\''"; // end quote, escape, reopen
        } else {
            out << c;
        }
    }
    return out.str();
}

// ---------- File helpers ----------

std::vector<std::string> read_lines(const fs::path &file) {
    std::vector<std::string> lines;
    std::ifstream in(file);
    if (!in.is_open()) {
        return lines;
    }
    std::string line;
    while (std::getline(in, line)) {
        const auto cleaned = trim(line);
        if (!cleaned.empty()) {
            lines.push_back(cleaned);
        }
    }
    return lines;
}

void ensure_file_exists(const fs::path &file) {
    if (fs::exists(file)) {
        return;
    }
    std::ofstream out(file);
    (void)out;
}

size_t read_progress(const fs::path &progress_file) {
    std::ifstream in(progress_file);
    if (!in.is_open()) {
        return 0;
    }
    size_t progress = 0;
    in >> progress;
    return progress;
}

void write_progress(const fs::path &progress_file, size_t next_index) {
    std::ofstream out(progress_file, std::ios::trunc);
    out << next_index;
}

void append_result_to_file(const fs::path &result_file, const std::string &text) {
    std::ofstream out(result_file, std::ios::app);
    out << text;
}

// ---------- Networking helpers ----------

size_t write_to_string(void *contents, size_t size, size_t nmemb, void *userp) {
    const size_t total_size = size * nmemb;
    auto *buffer = static_cast<std::string *>(userp);
    buffer->append(static_cast<char *>(contents), total_size);
    return total_size;
}

std::string download_url(const std::string &url) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return {};
    }

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || http_code >= 400) {
        return {};
    }
    return response;
}

std::string strip_html(const std::string &html) {
    std::string text;
    text.reserve(html.size());

    bool in_tag = false;
    bool in_script = false;
    bool in_style = false;
    for (size_t i = 0; i < html.size(); ++i) {
        char c = html[i];
        if (c == '<') {
            in_tag = true;
            // simple detection for script/style tags
            if (i + 7 < html.size() && (html.substr(i, 7) == "<style " || html.substr(i, 7) == "<style>")) {
                in_style = true;
            } else if (i + 8 < html.size() && (html.substr(i, 8) == "<script " || html.substr(i, 8) == "<script>")) {
                in_script = true;
            }
            continue;
        }
        if (c == '>' && in_tag) {
            in_tag = false;
            if (i >= 8 && html.substr(i - 7, 8) == "</style>") {
                in_style = false;
            } else if (i >= 9 && html.substr(i - 8, 9) == "</script>") {
                in_script = false;
            }
            if (!text.empty() && text.back() != ' ') {
                text.push_back(' ');
            }
            continue;
        }
        if (!in_tag && !in_script && !in_style) {
            text.push_back(c);
        }
    }
    return text;
}

// ---------- Ollama integration ----------

std::string run_ollama(const std::string &model, const std::string &prompt) {
    const std::string escaped_prompt = escape_shell_single_quotes(prompt);
    const std::string cmd = "sh -c 'echo '" + escaped_prompt + "' | ollama run " + model + "'";

    std::array<char, 256> buffer{};
    std::string result;

    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        return "Konnte Ollama nicht ausführen.";
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// ---------- JSON writer ----------

void write_json(const fs::path &json_file, const std::string &url, const std::string &page_text, const std::vector<PromptResult> &results) {
    std::ofstream out(json_file, std::ios::trunc);
    out << "{\n";
    out << "  \"url\": \"" << escape_json(url) << "\",\n";
    out << "  \"page_text\": \"" << escape_json(page_text) << "\",\n";
    out << "  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i) {
        out << "    {\n";
        out << "      \"prompt\": \"" << escape_json(results[i].prompt) << "\",\n";
        out << "      \"response\": \"" << escape_json(results[i].response) << "\"\n";
        out << "    }";
        if (i + 1 < results.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

// ---------- Prompt builder ----------

std::string build_prompt(const std::string &page_text, const std::string &user_prompt) {
    std::ostringstream oss;
    oss << "Nutze den folgenden Seiteninhalt als Kontext und beantworte präzise im gewünschten Format.\n";
    oss << "--- Kontext Anfang ---\n" << page_text << "\n--- Kontext Ende ---\n";
    oss << "Aufgabe: " << user_prompt << "\n";
    return oss.str();
}

// ---------- Processing pipeline ----------

void process_link(const Config &config, size_t link_index, const std::string &url, const std::vector<std::string> &prompts) {
    const auto parent_dir = config.link_file.parent_path().empty() ? fs::current_path() : config.link_file.parent_path();
    const fs::path json_file = parent_dir / ("link_" + std::to_string(link_index + 1) + ".json");

    std::vector<PromptResult> prompt_results;

    const std::string html = download_url(url);
    if (html.empty()) {
        std::cout << "Link " << (link_index + 1) << ": Webseite nicht vorhanden" << std::endl;
        append_result_to_file(config.result_file, "Link " + std::to_string(link_index + 1) + ": " + url + "\nWebseite nicht vorhanden\n\n\n");
        write_json(json_file, url, "", prompt_results);
        return;
    }

    const std::string page_text = strip_html(html);
    write_json(json_file, url, page_text, prompt_results);

    for (const auto &prompt_line : prompts) {
        const std::string full_prompt = build_prompt(page_text, prompt_line);
        const std::string response = run_ollama(config.model_name, full_prompt);

        std::cout << "Prompt: " << prompt_line << "\n";
        std::cout << "OLLAMA denken: " << response << "\n";
        std::cout << "Ergebnis: " << response << "\n";

        prompt_results.push_back({prompt_line, response});
        write_json(json_file, url, page_text, prompt_results);

        std::ostringstream result_line;
        result_line << "Link " << (link_index + 1) << ": " << url << "\n";
        result_line << "Prompt: " << prompt_line << "\n";
        result_line << "Ergebnis: " << response << "\n\n";
        append_result_to_file(config.result_file, result_line.str());
    }

    append_result_to_file(config.result_file, "\n\n");
}

Config build_config(int argc, char **argv) {
    Config config;
    if (argc > 1) {
        config.link_file = argv[1];
    } else {
        config.link_file = "links.txt";
    }

    const auto parent_dir = config.link_file.parent_path().empty() ? fs::current_path() : config.link_file.parent_path();
    config.prompt_file = parent_dir / "prompts.txt";
    config.progress_file = parent_dir / "progress.txt";
    config.result_file = parent_dir / "result.txt";
    return config;
}

int main(int argc, char **argv) {
    Config config = build_config(argc, argv);

    std::cout << "Link-Datei: " << config.link_file << std::endl;
    std::cout << "Prompt-Datei: " << config.prompt_file << std::endl;
    std::cout << "Modell: " << config.model_name << std::endl;

    ensure_file_exists(config.result_file);

    while (true) {
        const auto links = read_lines(config.link_file);
        const auto prompts = read_lines(config.prompt_file);

        if (links.empty()) {
            std::cout << "Keine Links gefunden. Warte " << config.interval_seconds << " Sekunden..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(config.interval_seconds));
            continue;
        }

        if (prompts.empty()) {
            std::cerr << "Keine Prompts gefunden. Bitte fülle " << config.prompt_file << std::endl;
            return 1;
        }

        size_t progress = read_progress(config.progress_file);
        if (progress >= links.size()) {
            std::cout << "Alle Links verarbeitet. Warte " << config.interval_seconds << " Sekunden auf neue Links..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(config.interval_seconds));
            continue;
        }

        for (size_t i = progress; i < links.size(); ++i) {
            process_link(config, i, links[i], prompts);
            write_progress(config.progress_file, i + 1);
        }
    }

    return 0;
}
