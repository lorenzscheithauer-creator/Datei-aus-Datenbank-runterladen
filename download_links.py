#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Automatisches Download-Skript für Link-Datei aus XAMPP-Umgebung.

FUNKTION:
- Liest regelmäßig (alle 15 Minuten) eine Datei mit Links ein.
- Prüft, welche Links bereits verarbeitet wurden.
- Lädt NUR NEUE Links herunter.
- Bereits verarbeitete Links werden NICHT erneut heruntergeladen.
- Merkt sich verarbeitete Links in einer separaten Datei.

ANNAHME:
- In der Quelldatei steht pro Zeile genau EIN Link (URL).
- Leere Zeilen und Zeilen mit nur Leerzeichen werden ignoriert.
- Die Datei wird außerhalb dieses Skripts (z.B. über XAMPP-Weboberfläche)
  immer wieder aktualisiert / ersetzt.

HINWEIS:
- Dieses Skript kann mit Python ausgeführt werden.
- Für Dauerbetrieb einfach laufen lassen (while True + sleep(900)).
"""

import os
import time
import urllib.parse
import urllib.request
from datetime import datetime

# ==============================
# KONFIGURATION
# ==============================

# Pfad zur Datei, die regelmäßig hochgeladen / aktualisiert wird
# (bitte anpassen!)
SOURCE_LINK_FILE = r"C:\xampp\htdocs\links\links.txt"

# Pfad zu einer Datei, in der ALLE bereits verarbeiteten Links gespeichert werden
# (bitte anpassen; die Datei wird automatisch angelegt, falls sie nicht existiert)
PROCESSED_LINKS_FILE = r"C:\xampp\htdocs\links\processed_links.txt"

# Ordner, in den die heruntergeladenen Dateien gespeichert werden sollen
# (bitte anpassen!)
DOWNLOAD_DIR = r"C:\xampp\htdocs\downloads"

# Intervall in Sekunden (15 Minuten = 900 Sekunden)
INTERVAL_SECONDS = 15 * 60  # 900


# ==============================
# HILFSFUNKTIONEN
# ==============================

def ensure_directories():
    """
    Stellt sicher, dass der Download-Ordner existiert.
    Legt ihn an, falls nicht.
    """
    if not os.path.isdir(DOWNLOAD_DIR):
        os.makedirs(DOWNLOAD_DIR, exist_ok=True)


def load_processed_links():
    """
    Lädt die Liste der bereits verarbeiteten Links aus PROCESSED_LINKS_FILE.

    Rückgabe:
        set[str]: Menge der bereits verarbeiteten Links.
    """
    if not os.path.isfile(PROCESSED_LINKS_FILE):
        return set()

    processed = set()
    with open(PROCESSED_LINKS_FILE, "r", encoding="utf-8") as f:
        for line in f:
            url = line.strip()
            if url:
                processed.add(url)
    return processed


def save_processed_links(processed_links):
    """
    Speichert alle verarbeiteten Links zurück in PROCESSED_LINKS_FILE.

    Args:
        processed_links (set[str]): Menge aller verarbeiteten Links.
    """
    with open(PROCESSED_LINKS_FILE, "w", encoding="utf-8") as f:
        for url in sorted(processed_links):
            f.write(url + "\n")


def read_links_from_source():
    """
    Liest alle Links aus der Quelldatei SOURCE_LINK_FILE.

    Rückgabe:
        list[str]: Liste der Links (ohne Duplikate in dieser Datei selbst,
                   gereinigt von Leerzeilen).
    """
    if not os.path.isfile(SOURCE_LINK_FILE):
        # Wenn die Datei (noch) nicht existiert, einfach leere Liste zurückgeben
        return []

    urls = []
    with open(SOURCE_LINK_FILE, "r", encoding="utf-8") as f:
        for line in f:
            url = line.strip()
            if not url:
                continue  # leere Zeilen ignorieren
            # Duplikate innerhalb derselben Datei vermeiden
            if url not in urls:
                urls.append(url)
    return urls


def build_filename_from_url(url):
    """
    Erzeugt einen Dateinamen aus der URL.

    Logik:
    - Nimmt den letzten Teil des Pfads als Basis (z.B. "bild.jpg").
    - Wenn kein Name eindeutig vorhanden ist, wird ein generischer Name mit Timestamp gebaut.
    - Entfernt ggf. verbotene Zeichen.

    Args:
        url (str): Die URL.

    Rückgabe:
        str: Dateiname (ohne Pfad).
    """
    parsed = urllib.parse.urlparse(url)
    path = parsed.path

    # Letztes Segment aus dem Pfad nehmen
    base_name = os.path.basename(path)

    # Falls der Name leer ist (z.B. URL endet mit /), generischer Name
    if not base_name:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        base_name = f"download_{timestamp}"

    # Unerwünschte Zeichen ersetzen
    invalid_chars = '<>:"/\\|?*'
    safe_name = "".join("_" if c in invalid_chars else c for c in base_name)

    return safe_name


def download_url(url):
    """
    Lädt eine einzelne URL herunter und speichert sie im DOWNLOAD_DIR.

    Args:
        url (str): URL, die heruntergeladen werden soll.

    Rückgabe:
        str | None: Pfad zur heruntergeladenen Datei bei Erfolg, sonst None.
    """
    try:
        filename = build_filename_from_url(url)
        target_path = os.path.join(DOWNLOAD_DIR, filename)

        # Falls gleiche Datei schon existiert: leicht abgewandelten Namen verwenden
        if os.path.exists(target_path):
            name, ext = os.path.splitext(filename)
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            new_filename = f"{name}_{timestamp}{ext}"
            target_path = os.path.join(DOWNLOAD_DIR, new_filename)

        # Download durchführen
        print(f"[{datetime.now().isoformat(timespec='seconds')}] Lade herunter: {url}")
        urllib.request.urlretrieve(url, target_path)
        print(f"[{datetime.now().isoformat(timespec='seconds')}] Gespeichert als: {target_path}")

        return target_path
    except Exception as e:
        print(f"[{datetime.now().isoformat(timespec='seconds')}] FEHLER beim Download von {url}: {e}")
        return None


# ==============================
# HAUPTLOGIK EINZELDURCHLAUF
# ==============================

def process_links_once():
    """
    Ein Durchlauf:
    - Lädt bereits verarbeitete Links.
    - Liest aktuelle Links aus der Quelldatei.
    - Bestimmt neue Links.
    - Lädt nur die neuen Links herunter.
    - Aktualisiert die Liste der verarbeiteten Links.
    """
    ensure_directories()

    processed_links = load_processed_links()
    current_links = read_links_from_source()

    print("=" * 60)
    print(f"[{datetime.now().isoformat(timespec='seconds')}] Starte neuen Durchlauf")
    print(f"Aktuell in Datei gefunden: {len(current_links)} Links")
    print(f"Bereits verarbeitete Links: {len(processed_links)}")

    # Neue Links bestimmen
    new_links = [url for url in current_links if url not in processed_links]

    print(f"Neue Links, die heruntergeladen werden: {len(new_links)}")

    # Nur neue Links herunterladen
    for url in new_links:
        result = download_url(url)
        if result is not None:
            processed_links.add(url)

    # Aktualisierte Liste speichern
    save_processed_links(processed_links)

    print(f"[{datetime.now().isoformat(timespec='seconds')}] Durchlauf beendet")
    print("=" * 60)


# ==============================
# HAUPTSCHLEIFE (ALLE 15 MIN)
# ==============================

if __name__ == "__main__":
    # Endlosschleife: Skript läuft dauerhaft und arbeitet alle 15 Minuten
    while True:
        process_links_once()
        # Warten bis zum nächsten Durchlauf
        time.sleep(INTERVAL_SECONDS)
