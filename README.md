# Datei aus Datenbank runterladen

Dieses Repository enthält ein kleines Python-Skript, das in einer XAMPP-Umgebung regelmäßig eine Textdatei mit Download-Links prüft, neue Einträge erkennt und die zugehörigen Dateien automatisch herunterlädt. Bereits verarbeitete Links werden in einer separaten Datei protokolliert, sodass derselbe Download nicht mehrfach durchgeführt wird.

## Konfiguration
- `SOURCE_LINK_FILE`: Pfad zur Textdatei mit den Links (z. B. `C:\xampp\htdocs\links\links.txt`).
- `PROCESSED_LINKS_FILE`: Pfad zur Datei, in der verarbeitete Links gespeichert werden (z. B. `C:\xampp\htdocs\links\processed_links.txt`).
- `DOWNLOAD_DIR`: Zielordner für die heruntergeladenen Dateien (z. B. `C:\xampp\htdocs\downloads`).
- `INTERVAL_SECONDS`: Wartezeit zwischen den Durchläufen (Standard: 900 Sekunden = 15 Minuten).

Passe die Pfade im Skript `download_links.py` an deine Umgebung an. Die Dateien werden automatisch angelegt, falls sie nicht existieren, und der Zielordner wird bei Bedarf erstellt.

## Nutzung
1. Stelle sicher, dass Python 3 installiert ist.
2. Passe die oben genannten Konstanten in `download_links.py` an.
3. Starte das Skript:
   ```bash
   python download_links.py
   ```
4. Das Skript läuft in einer Endlosschleife und prüft die Link-Datei alle 15 Minuten. Zum Beenden kannst du den Prozess mit `Strg+C` abbrechen.

## Hinweise
- Pro Zeile der Link-Datei wird genau eine URL erwartet. Leere Zeilen werden ignoriert.
- Wenn die Link-Datei in der XAMPP-Weboberfläche aktualisiert oder ersetzt wird, erkennt das Skript die neuen Links automatisch beim nächsten Durchlauf.
- Sind Dateien mit gleichem Namen bereits im Download-Ordner vorhanden, wird ein Zeitstempel angefügt, um Überschreibungen zu vermeiden.
