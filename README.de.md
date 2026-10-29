# Frame Mic Tuner

Ein SteamVR-Dashboard für das Steam Frame, mit dem du Echofilter und Rauschfilter des Headset-Mikrofons umschaltest, während du es trägst. Bei Ohrhörern den Echofilter aus, damit leise Geräusche wie Mundgeräusche klar durchkommen. Bei den Lautsprechern des Frame wieder an, damit ihr Klang nicht ins Mikrofon läuft. Ein eingebauter Stimmtest nimmt ein paar Sekunden von dem auf, was Apps wirklich hören, damit du die Einstellungen sofort vergleichen kannst.

[English](README.md) · [日本語](README.ja.md) · [简体中文](README.sc.md) · [Français](README.fr.md) · [Español](README.es.md) · [Italiano](README.it.md)

| Schnell (Presets) | Feinjustierung |
|---|---|
| ![Der Tab Schnell](docs/v12-en-quick-speaker_2026-10-18_19-50-00.png) | ![Der Tab Feinjustierung](docs/v12-en-fine_2026-10-18_19-50-00.png) |

### Demovideo (mit Ton)

Die Lautsprecher des Frame spielen ein Schneesturm-Geräusch, während der Stimmtest aufnimmt, und die beiden Aufnahmen werden abgespielt. 1. Clip: Rauschfilter aus (Preset Lautsprecher). 2. Clip: Echofilter ebenfalls aus (Preset Ohrhörer). Ohne Echofilter läuft der Schneesturm aus den Lautsprechern ins Mikrofon. Das Panel im Video ist auf Japanisch.

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## Was es tut

- **Einstellungen je Ausgang**: Das Panel merkt sich die Frame-Mikrofoneinstellungen (Echofilter, Rauschfilter und dessen Stärke) getrennt für jeden Audioausgang: die Lautsprecher des Frame, USB-C-Ohrhörer oder Adapter, Bluetooth-Kopfhörer. Wechselt der Ausgang (zum Beispiel steckst du Ohrhörer an und sie werden der Ausgang), legt die App die gespeicherte Einstellung dieses Ausgangs von selbst an, auch bei geschlossenem Panel. Das Panel sagt „<Ausgang> angeschlossen, also gilt die gespeicherte Einstellung“ und bietet **Rückgängig**. Ein noch unbekannter Ausgang startet mit dem Preset **Frame-Lautsprecher**, wenn es die Lautsprecher des Frame sind, sonst mit **Ohrhörer / Kopfhörer**.
- **Ausgang und Mikrofon**: Der Knopf **Ausgang** oben listet die verbundenen Ausgänge und die, die du früher benutzt hast. Dort kannst du **Hier wiedergeben** (macht ihn zum Standardausgang, dieselbe Einstellung wie die Audioeinstellung von SteamVR), einen Ausgang wählen, um seine Einstellungen im Voraus zu sehen und zu ändern, oder einen, den du nicht mehr benutzt, **Vergessen**. Die Liste scrollt mit dem Stick oder durch Ziehen. **Mikrofon** wählt, welches Mikrofon Apps benutzen. Echofilter und Rauschfilter gelten nur für das eingebaute Frame-Mikrofon. Bei einem anderen Mikrofon sagt das Panel das und bietet **Eingebautes Mikrofon benutzen**.
- **Schnell und Feinjustierung**: **Schnell** hat die Presets dafür, wie du hörst. **Ohrhörer / Kopfhörer** schaltet Echofilter und Rauschfilter aus. **Frame-Lautsprecher** schaltet den Echofilter ein und den Rauschfilter aus. **Feinjustierung** hat die einzelnen Ein/Aus-Schalter und zwei Regler für den Rauschfilter: **Strenge** (wie stimmähnlich ein Geräusch sein muss, um durchzukommen; niedriger lässt mehr durch) und **Nachklang** (wie lange Ton nach dem Sprechen noch durchkommt). Änderungen gelten sofort, auch während das Mikrofon in Gebrauch ist, ohne den Ton zu unterbrechen. Das Panel öffnet den Tab, den du zuletzt benutzt hast.
- **Signalweg**: zeigt, ob eine App das Mikrofon benutzt und durch welche Filter der Ton wirklich geht (Mikrofon → EQ → Echofilter → Rauschfilter → Apps), gelesen aus den laufenden PipeWire-Verbindungen.
- **Stumm**: Die Kopfzeile zeigt „In Gebrauch“, „Ungenutzt“ oder ein rotes „Stumm“ (stumm aus Steam, `wpctl`, einer Aux-Taste oder von anderswo) und direkt daneben **Stummschalten** / **Stummschaltung aus**.
- **Stimmtest**: nimmt bis zu 10 Sekunden des endgültigen Tons auf, den Apps bekommen, behält die letzten 5 Aufnahmen und spielt sie über den aktuellen Ausgang ab. Jede Aufnahme zeigt Uhrzeit, Länge, den Ausgang und die Einstellung, mit der sie gemacht wurde (zum Beispiel „AB13X · Echofilter aus“), und eine kleine Wellenform.
- **Apps & Updates** (Knopf oben rechts; ein rosa Punkt heißt, es gibt etwas zu sehen): die laufende Version, **Check now** und **Update**, der Schalter **Nach Updates suchen** und die anderen Steam-Frame-Apps des Autors. Die App, mit der diese hier zusammenarbeitet, frame-aux-shortcuts (schaltet die Stummschaltung mit der Aux-Taste um), steht zuerst. Die anderen sind einen Tipp entfernt. Installieren öffnet das Installationsprogramm in Konsole, nachdem dir der genaue Befehl gezeigt wurde.
- Oberfläche auf Japanisch, Englisch, vereinfachtem Chinesisch, Deutsch, Französisch, Spanisch und Italienisch. Text und Bedienelemente erfüllen WCAG 2.x AA beim Kontrast.

Warum das nötig ist: Solange eine App das Mikrofon benutzt, schickt SteamOS es durch EQ, Echofilter und Rauschfilter. Der Echofilter schwächt kleine Geräusche stark ab, und der Rauschfilter macht alles stumm, was nicht nach Stimme klingt. Bei Ohrhörern gibt es keinen Lautsprecherklang zum Wegrechnen. Ohne Echofilter kommt mehr von deiner Stimme durch.

## Voraussetzungen

- Ein Steam Frame mit Entwicklermodus und SSH (Einstellungen > System > Entwicklermodus, dann unter Entwickler ein Passwort setzen). Wähle ein starkes Passwort: Mit SSH kann sich jeder in deinem Netz, der es kennt, am Headset anmelden.
- SteamVR auf dem Headset. Bauen aus dem Quelltext (siehe unten) braucht cmake, ninja, g++, pkg-config und die Entwicklerdateien von cairo, FreeType, PipeWire und Vulkan. Die bringt SteamOS schon mit. Eine heruntergeladene Version braucht nichts davon.

## Installieren

### Am einfachsten: direkt im Frame installieren (empfohlen)

Kein PC nötig. In Konsole auf dem Frame (+ in der Leiste unten → die Programmliste → Konsole) diesen Befehl eintippen, Enter drücken und im Menü **3** (frame-mic-tuner) wählen.

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- Einmal vorher: Steam-Einstellungen → System → „Enable Developer Mode“ einschalten (solange das aus ist, erscheint Konsole nicht in der +-Liste).
- Die anderen Apps (frameeyeosc, frame-jp-keyboard, frame-perf-overlay, frame-aux-shortcuts) lassen sich aus demselben Menü installieren.
- Das Skript kannst du vorher lesen: `curl -fsSL https://frame.sasaken1102s.net/i | less`.
- Zum Aktualisieren denselben Befehl und dieselbe Nummer noch einmal. Zum Entfernen `u` im Menü.
- Schritt für Schritt und Video: https://frame.sasaken1102s.net
- Ohne Fragen: `curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

Was installiert wird und die Optionen sind dieselben wie unter „Von einem PC installieren“ (es führt `install.sh` für dich aus).

### Von einem PC installieren

Lade `frame-mic-tuner-<version>.tar.gz` von der [Release-Seite](https://github.com/sasaken1102r/frame-mic-tuner/releases) und kopiere es aufs Headset, zum Beispiel vom PC:

```sh
scp frame-mic-tuner-*.tar.gz steamos@<headset-ip>:
```

Dann auf dem Headset (`ssh steamos@<headset-ip>`):

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

Oder stattdessen aus dem Quelltext bauen (ebenfalls auf dem Headset):

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

Danach das **Headset einmal neu starten**, damit WirePlumber das Umschalt-Skript lädt. PipeWire oder WirePlumber nicht von Hand neu starten, während du spielst: SteamVR und Steam Link verlieren den Ton. Wenn das trotzdem passiert, SteamVR auf dem Headset neu starten (oder Steam Link neu verbinden), dann ist der Ton wieder da.

sudo ist nicht nötig. `install.sh` legt alles in dein Home-Verzeichnis (`~/.local/bin`, `~/.local/share`, `~/.config`), deshalb entfernen SteamOS-Updates es nicht. Von Hand aktualisieren: das neue Release laden und dessen `install.sh` noch einmal ausführen (oder in einem Quelltext-Checkout `git pull && ./install.sh`). Oder **Aktualisieren** in **Apps & Updates** drücken, sobald eine neuere Version da ist. Version 0.1.0 hat keinen Update-Knopf, deshalb muss der Schritt von 0.1.0 auf 0.2.0 dieses eine Mal von Hand geschehen.

Was installiert wird:

- die App, ein Eintrag für die **+**-Liste des Dashboards (Programm starten) und ihre Icons
- eine systemd-User-Unit, die sie zusammen mit SteamVR startet (nicht aktiv, bis du sie einschaltest; siehe unten)
- ein WirePlumber-Skript und eine Konfiguration (`contrib/wireplumber/`), die den Mikrofon-Tracker von SteamOS durch einen ersetzen, bei dem Echofilter und Rauschfilter umschaltbar sind. Sonst verhält er sich wie das Original.
- das gemeinsame Update-Skript (`~/.local/share/frame-mic-tuner/frame-update.sh`, aus `vendor/frame-updater/`), das **Apps & Updates** benutzt

Entfernen: `./install.sh --uninstall`, dann das Headset neu starten, damit das ursprüngliche Mikrofonverhalten von SteamOS zurückkommt. `--purge` löscht zusätzlich die Einstellungen der App und die gespeicherten Schalterwerte.

## Bedienung

1. Das SteamVR-Dashboard öffnen, **+** (Programm starten) drücken und **Frame Mic Tuner** wählen. In der Reihe unten im Dashboard erscheint ein **Mic**-Icon. Auswählen öffnet das Panel. Nochmal starten, während es läuft, öffnet nur das Panel.
2. Der Knopf **Ausgang** zeigt den Ausgang, über den du hörst. Im Tab **Schnell**, unter „Beim Hören über <Ausgang>“, **Ohrhörer / Kopfhörer** oder **Frame-Lautsprecher** wählen. Das sind Presets: Sie setzen Echofilter und Rauschfilter auf die empfohlenen Werte (die Chips auf jeder Karte zeigen sie). Die Stärke des Rauschfilters bleibt. Die App merkt sich die Wahl für diesen Ausgang, deshalb schaltet sie beim nächsten Mal von selbst. Für feinere Kontrolle zum Tab **Feinjustierung** wechseln. Passen deine Einstellungen dann zu keinem Preset, sagt **Schnell** „Gerade gelten die Einstellungen der Feinjustierung“. Der Rauschfilter schneidet leise Geräusche (Mundgeräusche und Ähnliches), wenn er an ist.
   - Im Tab **Feinjustierung** die Regler **Strenge** und **Nachklang** ziehen, um den Rauschfilter einzustellen. Die Vorgaben von SteamOS sind 23 % und 500 ms. **Standard** stellt sie zurück. Die Regler wirken nur, solange der Rauschfilter an ist.
   - SteamOS setzt diese beiden Werte zurück, wann immer sein Audio neu startet (zum Beispiel nach einem Neustart). Die App speichert deine Werte je Ausgang und legt sie beim Start und beim Ausgangswechsel wieder an. Sie gelten also nur, solange die App läuft. **Mit SteamVR starten** einschalten, wenn du sie immer willst. Ohne die App gelten die Vorgaben von SteamOS.
   - Nachdem die App die Einstellungen von selbst gewechselt hat, sagt **Schnell** „<Ausgang> angeschlossen, also gilt die gespeicherte Einstellung“. **Rückgängig** legt wieder an, was vorher galt, und merkt sich das stattdessen für diesen Ausgang.
3. **Ausgang** (oben links): öffnet „Ausgang wählen“. Eine Zeile drücken zeigt die Einstellungen dieses Ausgangs (du kannst die Einstellungen eines Ausgangs ändern, der gerade nicht verbunden ist; sie gelten, wenn er zurückkommt). **Hier wiedergeben** macht diesen Ausgang zum Standard, wie die Audioeinstellung von SteamVR. **Vergessen** löscht die gemerkten Einstellungen eines Ausgangs, der nicht verbunden ist. Mit dem Stick oder durch Ziehen scrollen.
4. **Mikrofon** (neben Ausgang): **Mikrofon nutzen** wählt das Mikrofon, von dem Apps aufnehmen. Bei allem außer dem eingebauten Frame-Mikrofon gelten Echofilter und Rauschfilter nicht, und die Einstellkarte bietet **Eingebautes Mikrofon benutzen**.
5. **Stummschalten** / **Stummschaltung aus** (oben, neben dem Status): schaltet das Standardmikrofon sofort stumm oder wieder an, ohne zu fragen.
6. **Stimmtest**: **Aufnehmen** drücken, etwas sagen und **Stopp** drücken (er stoppt nach 10 Sekunden von selbst). Die Einstellung wechseln, noch einmal aufnehmen und mit ▶ in jeder Zeile vergleichen. Die Aufnahme stoppt, sobald du das Panel schließt.
7. **Mit SteamVR starten** (untere Reihe): einschalten, damit die App ab jetzt automatisch mit SteamVR startet. Geht auch mit `./install.sh --autostart`.
8. **Sprache**: Das Panel startet in der Sprache deines Steam Frame (Japanisch, vereinfachtes Chinesisch, Deutsch, Französisch, Spanisch oder Italienisch, wenn Steam darauf steht, sonst Englisch). Unten links im Menü ändern (日本語, English, 简体中文, Deutsch, Français, Español, Italiano). Die Wahl wird gespeichert.
9. **Beenden**: zweimal drücken, oder über dem Mic-Icon im Dashboard schweben und Schließen wählen.
10. **Apps & Updates** (oben rechts; **Mikrofoneinstellungen** geht zurück). Die Texte und Knöpfe in dieser Box sind noch auf Englisch. **Diese App** zeigt die laufende Version, zum Beispiel „Up to date (0.3.4)“. **Check now** sucht sofort nach einem neueren Release (das geht auch, wenn **Nach Updates suchen** aus ist).
    - Wenn eines da ist, bekommt die Box einen Rahmen in der Akzentfarbe und sagt „Version X is available“. **Update** drücken. Es fragt „Update to X?“ mit **Cancel** und **Update** (die Frage verschwindet nach 3 Sekunden von selbst). Nochmal **Update** drücken, um zu starten. Die Box zeigt dann den Fortschritt („Updating: Downloading“ und so weiter).
    - Das Panel bleibt offen und läuft weiter mit der alten Version, während es installiert. Wenn es „X is installed. Quit and start the app again to use it“ sagt, **Beenden** zweimal drücken und die App wieder aus **+** starten (oder SteamVR neu starten), dann die Meldung mit **Close** schließen, falls sie noch da ist. Hat die neue Version das WirePlumber-Skript geändert, auch das Headset neu starten (die Box erinnert dich, und das Update-Log sagt, ob es sich geändert hat).
    - Sagt es „This version can't be installed from here“ (das Release hat kein `SHA256SUMS` oder kein Paket), von Hand aktualisieren, wie unter Installieren beschrieben. Die Box zeigt die Release-Seite.
    - Schlägt das Update fehl, bekommt die Box einen roten Rahmen, sagt warum und dass nichts geändert wurde, und bietet **Try again** und **Close**. Schlägt nur die Prüfung fehl (zum Beispiel ohne Netz), wird nur der Text rot und **Check now** bleibt.
    - **Nach Updates suchen** an / aus: aus stoppt die tägliche Prüfung und das Holen der App-Liste (siehe Datenschutz).
    - **Begleit-Apps** zeigt frame-aux-shortcuts (schaltet die Stummschaltung mit der Aux-Taste um) mit **Installieren**, wenn es fehlt. „Frame-Apps von sasaken@“ öffnet die ganze Liste. **Installieren** und **Installationsprogramm öffnen** zeigen zuerst den genauen Befehl. **In Konsole öffnen** führt ihn dann in Konsole aus, wo das Installationsprogramm den Fortschritt zeigt. Kein sudo. Alles landet im Home-Ordner.

Die Einstellungen gehen auch über SSH:

```sh
wpctl settings --save frame-mic.echo-cancel false        # Ohrhörer: Echofilter aus
wpctl settings --save frame-mic.echo-cancel true         # Lautsprecher: Echofilter an (Vorgabe)
wpctl settings --save frame-mic.noise-suppression true   # Rauschfilter an (Vorgabe: aus)
~/.local/bin/frame-mic-tuner --print                     # aktuelle Werte, Mikrofon in Gebrauch, Signalweg, Ausgänge und ihre gespeicherten Einstellungen
```

Eine Änderung auf diesem Weg (oder über eine Aux-Taste) fällt auf, solange das Panel offen ist, spätestens wenn der Ausgang wechselt, und wird für den aktuellen Ausgang gemerkt.

## Wenn etwas nicht geht

- **Das Panel sagt „Mikrofon-Umschaltung ist nicht installiert“**: Das WirePlumber-Skript ist noch nicht geladen. `./install.sh` ausführen, dann das Headset neu starten.
- **Die Autostart-Knöpfe sind grau**: Die systemd-Unit ist nicht installiert. `./install.sh` ausführen.
- **Aufnahmen sind stumm, oder andere hören dich gar nicht**: die Mikrofonlautstärke im Mixer des Headset prüfen (nur lesen, das ist sicher):
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  Beide sollten `values=96` sagen. Steht da `0`, ist das Mikrofon praktisch stumm. Diese App fasst den Mixer nie an, aber die Werte sind schon von selbst auf 0 gefallen. Zurücksetzen (auf eigene Gefahr; vorher die aktuellen Werte und das `max=` von `cget` notieren und den Wert nehmen, den dein Headset normal hat, bei uns 96):
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **Gar kein Ton in SteamVR oder Steam Link, nachdem PipeWire oder WirePlumber neu gestartet wurde**: SteamVR auf dem Headset neu starten (oder Steam Link neu verbinden). Der Ton kommt zurück.
- **Gar kein Ton nach einem SteamOS-Update**: Das Umschalt-Skript wird als nötiger Teil von WirePlumber geladen. Bricht ein Update es, kann WirePlumber stehen bleiben und den ganzen Ton mitnehmen. Die Konfiguration über SSH entfernen und das Headset neu starten (oder `./install.sh --uninstall` und neu starten):
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **„In Gebrauch“, obwohl du in keiner App sprichst**: Jede App, die Audio aufnimmt, zählt, auch Bildschirm- oder Audio-Recorder. SteamOS schaltet die Filter für alle davon ein.
- **Frame Mic Tuner fehlt in der +-Liste**: das Headset einmal nach dem Installieren neu starten.
- **Das Mic-Icon fehlt im Dashboard, obwohl die App läuft**: SteamVR kann das Dashboard-Panel der App verlieren (zum Beispiel wenn das Dashboard selbst neu startet). Die App prüft alle 3 Sekunden und legt das Panel selbst neu an. Ein paar Sekunden warten und das Dashboard wieder öffnen. Die App noch einmal aus **+** starten prüft und legt es ebenfalls neu an, bevor das Panel aufgeht. Kommt es trotzdem nicht zurück, sagt das Log warum (`journalctl --user -u frame-mic-tuner -f`, wenn es als Dienst läuft). Das Log ist auf Japanisch. Zeilen, die mit „自己修復“ beginnen, heißen „Selbstreparatur“. Die App beenden und neu starten behebt es auch.
- Logs: `journalctl --user -u frame-mic-tuner -f`, wenn es als Dienst läuft. Das Log eines manuellen Starts siehst du, indem du die App beendest und `~/.local/bin/frame-mic-tuner` aus SSH startest.
- **Die Update-Prüfung schlägt fehl, oder „Update“ tut nichts**: `~/.cache/frame-mic-tuner/update.log` hat die Einzelheiten, `~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` die letzte Roh-Antwort. **Nach Updates suchen** aus (oder `update_check: false` in der Einstellungsdatei) stoppt die automatische tägliche Prüfung, ohne **Check now** / **Update** zu entfernen.
- **Die Einstellungen haben nicht gewechselt, als ich Ohrhörer angesteckt habe**: Die App folgt nur dem *Standard*-Ausgang. Spielt SteamVR weiter über die Lautsprecher des Frame, bei den Ohrhörern in **Ausgang** **Hier wiedergeben** drücken (oder sie in der Audioeinstellung von SteamVR wählen). Ausgänge, die keine echten Geräte sind (Filter, virtuelle Ausgänge), werden ignoriert.
- **„Konsole ließ sich nicht öffnen“ beim Installieren einer anderen App**: Konsole braucht den Entwicklermodus (Steam-Einstellungen → System → Enable Developer Mode).

## Datenschutz

- Aufnahmen des Stimmtests bleiben nur im Speicher der App. Sie werden nie auf die Platte oder in Logs geschrieben und nie irgendwohin geschickt. Sie sind weg, wenn die App endet.
- Während der Aufnahme zeigt das Panel „● Aufnahme“. Die Aufnahme stoppt, sobald du das Panel schließt.
- Das Mikrofon kann Stimmen von Leuten um dich herum aufnehmen. Daran denken, bevor du aufnimmst.
- Für die Standardsprache liest es beim Start einmal die Zeile `language` in Steams `~/.steam/registry.vdf` (nur lesen).
- Die App hat keine Telemetrie. Netz nutzt sie für die Update-Prüfung und die App-Liste (unten):
  - **Wann**: Solange die App läuft, fragt sie GitHub beim Start, wenn die letzte Antwort älter als 24 Stunden ist, und danach ungefähr einmal am Tag (eine Stunde später nach einer fehlgeschlagenen Prüfung). **Jetzt prüfen** fragt sofort. **Nach Updates suchen** in **Apps & Updates** aus (oder `update_check: false` in der Einstellungsdatei) stoppt die automatische Prüfung. Dann verbindet sie sich nur, wenn du **Jetzt prüfen** oder **Aktualisieren** drückst.
  - **Was sie schickt**: eine HTTPS-Anfrage nach dem neuesten Release dieses Projekts an `api.github.com`, ohne Konto, ID oder Nutzungsdaten. Wie bei jeder Web-Anfrage sieht GitHub deine IP-Adresse und den User-Agent von `curl`. Aufnahmen, Einstellungen und Logs werden nie geschickt.
  - **Was sie bekommt**: Versionsnummer des neuesten Release, seine Seite und die Namen und Links seiner Dateien. Die Antwort liegt in `~/.cache/frame-mic-tuner/update-check.json`.
  - **Nur wenn du Update drückst und bestätigst**, lädt sie außerdem das Paket dieses Release und `SHA256SUMS` von `github.com` / `*.githubusercontent.com`.
- Ist **Nach Updates suchen** an, holt sie auch die Liste der anderen Steam-Frame-Apps des Autors und ihre Icons von `frame.sasaken1102s.net` (höchstens einmal pro Stunde, geteilt mit den anderen Apps des Autors, abgelegt in `~/.cache/frame-apps/`). Das ist ein schlichtes HTTPS-GET ohne Anhang (`curl -q --proto =https --max-redirs 0`): Wie GitHub sieht `frame.sasaken1102s.net` deine IP-Adresse und den User-Agent von `curl`, und sonst wird nichts geschickt. **Nach Updates suchen** aus stoppt auch das. Das Panel zeigt dann die zuletzt geholte Liste oder die, die in der App steckt. Installierst du eine andere App aus **Apps & Updates**, öffnet sich Konsole und führt den auf der Bestätigung gezeigten Installer-Befehl aus, der sich innerhalb von Konsole mit `frame.sasaken1102s.net` und GitHub verbindet.
- Die einzigen Dateien, die sie schreibt, sind `~/.config/frame-mic-tuner/config.json` (UI-Sprache, letzter Tab, die Update-Prüfung und für jeden benutzten Ausgang dessen Name, die Frame-Mikrofoneinstellungen und das Datum der letzten Nutzung), `~/.config/frame-mic-tuner/install-args` (die Optionen deines letzten `./install.sh`, damit ein Update genauso neu installiert), `~/.cache/frame-mic-tuner/` (die zwischengespeicherte Antwort der Update-Prüfung, Log und Zustand des Updates, ein Sperrordner `update.lock/` während ein Update läuft, und ein Arbeitsordner, dessen Download nach jedem Update gelöscht wird), `~/.cache/frame-apps/` (die geholte App-Liste und Icons, geteilt mit den anderen Apps des Autors) und eine Sperrdatei in `$XDG_RUNTIME_DIR`, die ihre Prozess-ID hält (damit keine zweite Kopie startet). Ein Update schreibt auch die installierten Dateien neu, so wie `./install.sh`.

## Hinweis

- Benutzung auf eigene Gefahr. Dieses Projekt wurde mit Claude Opus 5.5 gemacht, einem KI-Modell. Ich habe es auf meinem eigenen Steam Frame geprüft, kann aber nicht dafür einstehen, was auf deinem passiert. Lies den Code und prüfe ihn selbst, bevor du ihn ausführst. Die Software kommt ohne Gewähr (siehe [LICENSE](LICENSE)).
- Was es ändert: die beiden WirePlumber-Einstellungen `frame-mic.echo-cancel` und `frame-mic.noise-suppression` (über `wpctl settings`), die beiden laufenden Parameter des Rauschfilters „VAD Threshold (%)“ und „VAD Grace Period (ms)“ (über `pw-cli set-param`; SteamOS legt seine Vorgaben zurück, wenn sein Audio neu startet) und die WirePlumber-Konfiguration in deinem Home-Verzeichnis, wo es den Mikrofon-Tracker von SteamOS durch sein eigenes Skript ersetzt. Wenn du **Hier wiedergeben**, **Mikrofon nutzen** / **Eingebautes Mikrofon benutzen** oder **Stummschalten** / **Stummschaltung aus** drückst, ändert es den Standardausgang, das Standardmikrofon oder dessen Stummschaltung (über `wpctl set-default` / `wpctl set-mute`, wie die Audioeinstellung von SteamVR). Darüber hinaus aktiviert oder deaktiviert es nur seine eigene systemd-User-Unit, wenn du das verlangst, öffnet Konsole mit dem Installer, wenn du das Installieren einer anderen App bestätigst, und führt beim Update aus dem Panel das `install.sh` des neuen Release in einer kurzlebigen systemd-User-Unit aus (`frame-mic-tuner-update`).
- Es startet PipeWire oder WirePlumber nie neu, schreibt nie in den ALSA-Mixer (amixer), fasst `/etc` nie an und braucht nie root.
- **Update**: lädt `.tar.gz` und `SHA256SUMS` des neuesten Release über HTTPS von GitHub (nur zu GitHubs eigenen Hosts), prüft den Hash und entpackt erst dann nach `~/.cache/frame-mic-tuner/update/` und führt dessen `install.sh` mit denselben Optionen wie deine letzte Installation aus. Es lehnt ein Release ohne `SHA256SUMS`, einen Hash, der nicht passt, oder ein Archiv mit einem absoluten Pfad, `..`, einem Link oder einer Spezialdatei ab. In all diesen Fällen wird nichts geändert. Der Download wird danach gelöscht. `SHA256SUMS` ist eine Prüfsumme, die im selben Release veröffentlicht wird, keine Signatur: Sie erkennt einen beschädigten oder unvollständigen Download, aber kein Release, das auf GitHub ersetzt wurde. Die laufende App wird nicht neu gestartet: Beende sie und starte sie neu, um die neue Version zu benutzen. Die genauen Schritte stehen in den [eigenen Notizen von frame-update.sh](vendor/frame-updater/frame-update.sh).
- Ein SteamOS-Update kann ändern, wie die Mikrofonfilter eingerichtet sind. Dann können die Schalter nicht mehr wirken, bis dieses Projekt aktualisiert ist. Weil das Umschalt-Skript ein nötiger Teil von WirePlumber ist, kann ein Skript, das nicht mehr lädt, auch den ganzen Ton auf dem Headset stoppen (siehe Wenn etwas nicht geht). `./install.sh --uninstall` und ein Neustart bringen das ursprüngliche Verhalten von SteamOS zurück.
- Das ist ein inoffizielles Projekt, ohne Verbindung zu oder Unterstützung durch die Valve Corporation. Steam, Steam Frame, SteamOS, SteamVR und Steam Link sind Marken und/oder eingetragene Marken der Valve Corporation in den USA und/oder anderen Ländern. Die Namen stehen hier nur, um zu sagen, womit das hier zusammenarbeitet.

## Entwicklung

Auf dem Headset bauen:

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # Release-Build: dist/frame-mic-tuner-<version>.tar.gz, dist/SHA256SUMS
```

Nützliche Optionen, die kein SteamVR brauchen: `--print`, `--set-ns-vad N` / `--set-ns-grace N` (eine Rauschfilter-Stärke sofort anlegen, ohne sie zu speichern), `--dump-png PATH` (das Panel zeichnen, mit `--fake-*`-Zuständen für Screenshots, etwa `--fake-output`, `--fake-switched`, `--fake-external-mic`, `--fake-update STATE`, und `--view` / `--overlay` für die anderen Bildschirme), `--test-record 3` (3 Sekunden aufnehmen und abspielen), `--contrast-report` (WCAG-Kontrast jedes Farbpaars) und `--version`. Entwurfsnotizen, wie das Umschalt-Skript funktioniert, und Testergebnisse stehen in [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md) (Japanisch).

Der Update-Mechanismus (`vendor/frame-updater/`), die gemeinsamen UI-Teile (`vendor/frame-ui/`) und die Liste der anderen Apps des Autors (`vendor/frame-apps/`) sind Kopien aus gemeinsamen, privaten Repositories. Die Kopien nicht von Hand ändern. Deren `verify.sh` (ausgeführt von `scripts/package.sh`) prüft, dass sie nicht geändert wurden.

Release-Builds behalten die Geschwindigkeitsoptimierung des Compilers und nutzen, wenn verfügbar, Link-Time-Optimierung sowie das Entfernen ungenutzten Codes und ungenutzter Daten. Nach dem Bauen führt `ctest --test-dir build --output-on-failure` den bisherigen `--self-test` und die vollständigen Tests der Pixelumwandlung aus, ohne SteamVR oder Audio-Hardware. `./build/rgba-test --benchmark` vergleicht außerdem die Umwandlungszeit mit dem vorherigen Verfahren. Tests sind nicht in den Release-Paketen. Mit `-DBUILD_TESTING=OFF` beim Konfigurieren werden sie nicht gebaut.

### Veröffentlichen

`scripts/package.sh` führt das `verify.sh` der drei Kopien unter `vendor/` aus, baut ein Release-Binary und schreibt `dist/frame-mic-tuner-<version>.tar.gz` und `dist/SHA256SUMS` (nötig, damit der Knopf **Aktualisieren** im Panel das Release annimmt). Es gibt den Befehl aus, der das GitHub-Release (und dessen Tag) mit beiden Dateien anlegt. Die Release-Notizen vorher in eine Datei außerhalb des Repository schreiben:

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <release-notes-file>
```

Kein Entwurf und kein Vorab-Release: Das Panel schaut auf GitHubs neuestes Release, und das überspringt beides.

## Lizenz

MIT. Siehe [LICENSE](LICENSE). Der mitgelieferte OpenVR-Header (`third_party/openvr/`) ist BSD-3-Clause von der Valve Corporation. Das Sprachsymbol (`assets/heroicons-language-16-solid.svg`, auch ins Programm eingebaut) stammt aus [Heroicons](https://github.com/tailwindlabs/heroicons) von Tailwind Labs, MIT. `vendor/frame-updater/`, `vendor/frame-ui/` und `vendor/frame-apps/` (einschließlich der Icons der anderen Apps, die ins Programm eingebaut sind) sind Kopien des eigenen Codes des Autors und sind MIT wie der Rest dieses Repository. Lizenzen des mitgelieferten Headers und der Bibliotheken, die er benutzt, stehen in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Änderungen stehen in [CHANGELOG.md](CHANGELOG.md).
