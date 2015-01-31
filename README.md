# ToxBot
Der Tox-Bot ist ein [Tox](https://tox.im) Bot, der dazu da ist, Freunde automatisch in Gruppen einzuladen. Er nimmt Einladungen automatisch an und lädt diese in die Gruppe ein(Standard ist ohne eigene Änderung Gruppe 0). Er kann auch Gruppen erstellen, verlassen, mit Passwort geschützten Gruppen umgehen, Gruppen verwalten und Nachrichten in Gruppen senden.

Die Funktionalität ist momentan recht spartanisch und einfach zu Erweitern, bis Gruppen-Administrationen in Tox möglich sind.

## Handhabung
Um den Bot zu Verwalten, muss die Tox-ID in die masterkey-Datei eingetragen werden. Sobald der Bot dann als Freund hinzugefügt wurde, kann man ihm [Administratoren-Befehle](https://github.com/JFreegman/ToxBot/blob/master/commands.txt) schicken, wie normale Nachrichten. Es gibt keine graphische Oberfläche.

Bemerkung: ToxBot akzeptiert Gruppen-Einladungen des Administrators automatisch.

## Start-Optionen [NEU!]
* `-h` oder `--help` - Zeigt die Hilfe
*  `-b` oder `--background` - Führt den Bot im Hintergrund aus. Der Status wird über die tty ausgegeben.
*  `-a [ID]` oder `--addmaster [ID]` - Gibt der ID Admin-Rechte für den Bot

### Non-Admin Befehle
* `hilfe` - Zeigt diese Nachricht
* `info` - Zeigt den Status des Bots
* `id` - Zeigt die Tox-ID des Bots
* `hallo` - Lädt dich in den bestehenden Gruppen-Chat
* `hallo <n> <pass>` - Lädt dich in eine mit einem Passwort geschütze Gruppe ein

## Anhängigkeiten
pkg-config
[libtoxcore](https://github.com/irungentoo/toxcore)
libtoxav

## Kompilieren
Führe `make` im Terminal aus

Bemerkung: Wenn der Fehler `cannot open shared object file: No such file or directory` erscheint, versuche `sudo ldconfig` auszuführen.


Dieses Projekt ist ein Fork von: https://github.com/JFreegman/ToxBot
