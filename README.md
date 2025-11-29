# Link-Verarbeitung mit Ollama (C++)

Dieses Programm liest eine Link-Liste zeilenweise ein, lädt jede Webseite herunter, übergibt den bereinigten Inhalt an Ollama (z. B. `deepseek-r1:7b`) und speichert alle Ergebnisse nachvollziehbar in JSON- sowie Text-Dateien. Der Fortschritt wird in einer separaten Datei abgelegt, sodass das Programm nach einem Neustart fortfahren kann. Alle Schritte sind in kleine, wiederverwendbare Unterprogramme aufgeteilt.

## Voraussetzungen
- Linux mit g++ (C++17)
- `libcurl` (z. B. Paket `libcurl4-openssl-dev`)
- Ollama mit installiertem Modell `deepseek-r1:7b` (GPU-Nutzung entsprechend deiner Ollama-Konfiguration)

## Dateien
- `links.txt`: Eine URL pro Zeile. Die Datei kann nummerierte oder reine URL-Zeilen enthalten.
- `prompts.txt`: Eine Aufgabe pro Zeile; jede Zeile wird als eigener Prompt an Ollama geschickt.
- `progress.txt`: Wird automatisch im gleichen Ordner angelegt und speichert die zuletzt verarbeitete Zeilennummer (0-basiert).
- `result.txt`: Wird automatisch im gleichen Ordner angelegt und sammelt alle Ollama-Antworten in chronologischer Reihenfolge.
- `link_<nr>.json`: Pro Link eine JSON-Datei mit Seiteninhalt und allen Prompt-Ergebnissen.

## Kompilieren
```bash
g++ -std=c++17 main.cpp -lcurl -o link_processor
```

## Nutzung
1. Passe (oder erstelle) `links.txt` und `prompts.txt` im selben Ordner an.
2. Starte das Programm wahlweise mit einem expliziten Pfad zur Link-Datei:
   ```bash
   ./link_processor /pfad/zu/links.txt
   ```
   Ohne Argument wird automatisch `links.txt` im aktuellen Ordner genutzt. Die übrigen Dateien (`prompts.txt`, `progress.txt`, `result.txt`) werden im gleichen Ordner erwartet bzw. angelegt.
3. Ablauf pro Link:
   - Webseite laden (Bilder etc. werden ignoriert, der reine Text wird extrahiert).
   - Inhalt in `link_<nr>.json` speichern.
   - Jeden Prompt aus `prompts.txt` mit Kontext an Ollama senden.
   - Terminalausgabe: `Prompt`, `OLLAMA denken`, `Ergebnis`.
   - Ergebnisse in `result.txt` und in die JSON-Datei anhängen.
   - Bei fehlender Seite wird „Webseite nicht vorhanden“ ausgegeben und protokolliert.
4. Polling: Wenn alle Links verarbeitet sind, wartet das Programm 15 Minuten und prüft dann erneut, ob neue Links hinzugekommen sind.

## Hinweise
- Der Fortschritt wird nach jedem Link gespeichert (`progress.txt`), sodass ein Neustart fortsetzt, ohne bereits erledigte Links zu duplizieren.
- `result.txt` erhält nach jedem Link zwei Leerzeilen, damit die Abschnitte klar getrennt bleiben.
- Die JSON-Dateien werden bei jedem Prompt-Schritt vollständig neu geschrieben, damit alle bisherigen Ergebnisse erhalten bleiben.
- Soll ein anderes Modell genutzt werden, passe `model_name` in `Config` (in `main.cpp`) an oder erweitere die Argument-Logik entsprechend.
