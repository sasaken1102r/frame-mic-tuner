# Frame Mic Tuner

A SteamVR dashboard panel for the Steam Frame that switches the headset microphone's echo cancellation and noise suppression while you're wearing it. Turn echo cancellation off when you use earphones, so quiet sounds like mouth noises come through clearly, and back on when you use the Frame's speakers, so their sound doesn't leak into your mic. A built-in voice check records a few seconds of what apps actually hear, so you can compare settings on the spot.

[日本語版はこちら](README.ja.md)

| Quick (presets) | Fine-tune |
|---|---|
| ![The Quick tab](docs/v11-en-quick-speaker_2026-10-10_17-49-17.png) | ![The Fine-tune tab](docs/v11-en-fine_2026-10-10_17-49-17.png) |

### Demo video (with sound)

The Frame's speakers play a blizzard sound while the voice check records, and the two recordings are played back. 1st clip: noise suppression off (Speaker preset). 2nd clip: echo cancellation off too (Earphones preset). With echo cancellation off, the blizzard from the speakers leaks into the mic. The panel in the video is in Japanese.

https://github.com/user-attachments/assets/14b5e180-f417-4d90-85f5-6ba1dc573546

## What it does

- **Settings for each output**: the panel remembers the Frame mic settings (echo cancellation, noise suppression and its strength) separately for each audio output: the Frame's speakers, USB-C earphones or adapters, Bluetooth headphones. When the output changes (for example you plug in earphones and they become the output), the app puts that output's settings on by itself, even with the panel closed, and the panel says "<output> connected, so its saved settings are on" with **Undo**. An output it hasn't seen before starts from the **Frame speakers** preset if it is the Frame's speakers, and from **Earphones / headphones** otherwise.
- **Output and Mic**: the **Output** button at the top lists the connected outputs and the ones you used before. There you can **Play sound here** (makes it the default output, the same setting as SteamVR's audio settings), pick any output to see and change its settings ahead of time, or **Forget** one you don't use any more. The list scrolls with the controller stick or by dragging. **Mic** picks which mic apps use. Echo cancellation and noise suppression only work on the Frame's built-in mic, so with another mic the panel says so and offers **Use the built-in mic**.
- **Two tabs**: **Quick** has the presets for how you listen. **Earphones / headphones** sets echo cancellation off and noise suppression off; **Frame speakers** sets echo cancellation on and noise suppression off. **Fine-tune** has the individual on/off switches and two sliders for the noise filter: **Strictness** (how voice-like a sound must be to pass; lower lets more through) and **Hold** (how long sound keeps passing after you stop speaking). Changes apply at once, even while the mic is in use, without cutting the sound. The panel opens on the tab you used last.
- **Signal path**: shows whether an app is using the mic and which filters the sound actually goes through (mic → EQ → echo cancellation → noise suppression → apps), read from the live PipeWire links.
- **Mute**: the header shows "In use", "Not in use" or a red "Muted" (muted from Steam, `wpctl`, an aux button or anywhere else), and a **Mute** / **Unmute** button right next to it.
- **Voice check**: records up to 10 seconds of the final sound that apps receive, keeps the last 5 recordings, and plays them back through the current output. Each recording shows the time, length, the output and setting it was made with (for example "AB13X · Echo cancel off") and a small waveform.
- **Apps & updates** (button at the top right; a pink dot means there's something to look at): the running version, **Check now** and **Update**, the **Check for updates** switch, and the author's other Steam Frame apps. The one this app works with, frame-aux-shortcuts (toggles the mic mute with the aux button), is listed first; the others are one tap away. Installing opens the installer in Konsole after showing you the exact command.
- Japanese, English, and Simplified Chinese UI. Text and controls meet WCAG 2.x AA contrast.

Why this is needed: while an app uses the mic, SteamOS runs it through EQ, echo cancellation and noise suppression. Echo cancellation strongly reduces small sounds, and noise suppression silences anything that doesn't sound like a voice. With earphones there is no speaker sound to cancel, so turning echo cancellation off lets more of your voice through.

## Requirements

- A Steam Frame with Developer Mode on and SSH access (Settings > System > Developer Mode, then set a password under Developer). Choose a strong password: with SSH on, anyone on your network who knows it can log in to the headset.
- SteamVR on the headset. Building from source (see below) needs cmake, ninja, g++, pkg-config and the cairo, FreeType, PipeWire and Vulkan development files, which already come with SteamOS; a downloaded release doesn't need any of them.

## Install

### Easiest: install right inside the Frame (recommended)

No PC needed. In Konsole on the Frame (+ on the bar at the bottom → the list of programs → Konsole), type this command, press Enter, and pick **3** (frame-mic-tuner) from the menu.

```sh
curl -fsSL https://frame.sasaken1102s.net | sh
```

- Do this once first: Steam Settings → System → turn on "Enable Developer Mode" (while it's off, Konsole doesn't show up in the + list).
- The other apps (frameeyeosc, frame-jp-keyboard, frame-perf-overlay, frame-aux-shortcuts) can be installed from the same menu.
- You can read the script before running it: `curl -fsSL https://frame.sasaken1102s.net/i | less`.
- To update, run the same command and pick the same number again. To uninstall, use `u` in the menu.
- Step-by-step guide and video: https://frame.sasaken1102s.net
- To install without any prompts: `curl -fsSL https://frame.sasaken1102s.net | sh -s -- install mic`

What gets installed and the options are the same as in "Install from a PC" below (it runs `install.sh` for you).

### Install from a PC

Download `frame-mic-tuner-<version>.tar.gz` from the [releases page](https://github.com/sasaken1102r/frame-mic-tuner/releases) and copy it to the headset, for example from your PC:

```sh
scp frame-mic-tuner-*.tar.gz steamos@<headset-ip>:
```

Then on the headset (`ssh steamos@<headset-ip>`):

```sh
tar xzf frame-mic-tuner-*.tar.gz
cd frame-mic-tuner
./install.sh
```

Or build it from source instead (also on the headset):

```sh
git clone https://github.com/sasaken1102r/frame-mic-tuner.git
cd frame-mic-tuner
./install.sh
```

Then **restart the headset** once, so WirePlumber loads the switch script. Don't restart PipeWire or WirePlumber by hand while playing: SteamVR and Steam Link lose their sound. If that happens anyway, restart SteamVR on the headset (or reconnect Steam Link) and the sound comes back.

No sudo is needed. `install.sh` puts everything into your home directory (`~/.local/bin`, `~/.local/share`, `~/.config`), so SteamOS updates don't remove it. To update by hand: download the new release and run its `install.sh` again (or, in a source checkout, `git pull && ./install.sh`) — or just press **Update** in the panel's **Apps & updates** once it shows a newer version is available. Version 0.1.0 has no update button, so going from 0.1.0 to 0.2.0 has to be done by hand this once.

What gets installed:

- the app, a launcher entry for the dashboard's **+** (launch a program) list, and its icons
- a systemd user unit for starting it together with SteamVR (not enabled unless you turn it on; see below)
- a WirePlumber script and config (`contrib/wireplumber/`) that replace SteamOS's microphone tracker with one whose echo cancellation and noise suppression can be switched. It behaves like the original otherwise.
- the shared update script (`~/.local/share/frame-mic-tuner/frame-update.sh`, from `vendor/frame-updater/`), used by **Apps & updates**

To remove it: `./install.sh --uninstall`, then restart the headset to get SteamOS's original microphone behaviour back. Add `--purge` to also delete the app's settings and the saved switch values.

## Usage

1. Open the SteamVR dashboard, press **+** (launch a program) and choose **Frame Mic Tuner**. A **Mic** icon appears in the row at the bottom of the dashboard; select it to open the panel. Launching it again while it's running just opens the panel.
2. The **Output** button shows the output you're listening on. On the **Quick** tab, under "When listening on <output>", choose **Earphones / headphones** or **Frame speakers**. These are presets: they set echo cancellation and noise suppression to the recommended values (the chips on each card show them); the noise filter strength stays as it is. The app remembers the choice for that output, so the next time the output comes back it switches by itself. For finer control, switch to the **Fine-tune** tab; if your settings then match neither preset, the Quick tab says "Using the Fine-tune settings now". Noise filter cuts quiet sounds (mouth noises and the like) when on.
   - On the **Fine-tune** tab, drag the **Strictness** and **Hold** sliders to tune the noise filter. SteamOS's defaults are 23% and 500 ms; **Default** brings them back. The sliders only have an effect while Noise filter is on.
   - SteamOS resets these two values whenever its audio restarts (for example after a reboot). The app saves your values for each output and applies them again when it starts and when the output changes, so they only stick while the app is running. Turn on **Start with SteamVR** if you want them all the time. Without the app, SteamOS's defaults are used.
   - After the app switched the settings by itself, the Quick tab says "<output> connected, so its saved settings are on". **Undo** puts back what was on before and remembers that for this output instead.
3. **Output** (top left): opens "Choose the output". Press a row to show that output's settings (you can change the settings of an output that isn't connected now; they are used when it comes back). **Play sound here** makes that output the default, as SteamVR's audio settings do. **Forget** deletes the remembered settings of an output that isn't connected. Scroll with the stick or by dragging.
4. **Mic** (next to Output): **Use this mic** picks the mic apps record from. With anything other than the Frame's built-in mic, echo cancellation and noise suppression don't apply, and the settings card offers **Use the built-in mic**.
5. **Mute** / **Unmute** (top, next to the status): mutes or unmutes the default mic at once, without asking.
6. **Voice check**: press **Record**, say something, and press **Stop** (it stops by itself after 10 seconds). Switch the setting, record again, and use ▶ on each row to compare. Recording stops as soon as you close the panel.
7. **Start with SteamVR** (bottom row): turn it on to have the app start automatically with SteamVR from now on. You can also enable it with `./install.sh --autostart`.
8. **Language**: the panel starts in your Steam Frame's language (Japanese for Japanese, Simplified Chinese for Simplified Chinese, English otherwise). Change it with **日本語 / English / 简体中文** at the bottom left; your choice is saved.
9. **Quit**: press it twice, or hover over the Mic icon in the dashboard and choose Close.
10. **Apps & updates** (top right; **← Mic settings** goes back). **This app** shows the running version, for example "Up to date (0.3.0)". **Check now** looks for a newer release right away (this works even if **Check for updates** is off).
    - When one is available, the box gets an accent-colored border and says "Version X is available". Press **Update**; it asks "Update to X?" with **Cancel** and **Update** (the question goes away by itself after 3 seconds). Press **Update** again to start. The box then shows the progress ("Updating: Downloading", and so on).
    - The panel stays open and keeps running the old version while it installs. When it says "X is installed. Quit and start the app again to use it", press **Quit** twice and start the app again from **+** (or restart SteamVR), then **Close** the message if it's still shown. If the new version changed the WirePlumber script, restart the headset too (the box reminds you, and the update log says whether it changed).
    - If it says "This version can't be installed from here" (the release has no `SHA256SUMS` or no package), update by hand as described under Install; the box shows the release page.
    - If the update fails, the box gets a red border, says why and that nothing was changed, and offers **Try again** and **Close**. If only the check fails (for example with no network), just the text turns red and **Check now** stays.
    - **Check for updates** on / off: off stops the daily check and the fetching of the app list (see Privacy).
    - **Companion apps** shows frame-aux-shortcuts (toggles the mic mute with the aux button) with **Install** when it's missing. "Frame apps by sasaken@" opens the full list. **Install** and **Open the installer** first show the exact command; **Open in Konsole** then runs it in Konsole, where the installer shows its progress. No sudo; everything goes into your home folder.

The settings are also available from SSH:

```sh
wpctl settings --save frame-mic.echo-cancel false        # earphones: echo cancellation off
wpctl settings --save frame-mic.echo-cancel true         # speaker: echo cancellation on (default)
wpctl settings --save frame-mic.noise-suppression true   # noise suppression on (default: off)
~/.local/bin/frame-mic-tuner --print                     # current values, mic in use, signal path, outputs and their saved settings
```

A change made this way (or by an aux button) is noticed while the panel is open, or at the latest when the output changes, and is remembered for the current output.

## Troubleshooting

- **The panel says "Mic switch is not installed"**: the WirePlumber script isn't loaded yet. Run `./install.sh`, then restart the headset.
- **Autostart buttons are grey**: the systemd unit isn't installed. Run `./install.sh`.
- **Recordings are silent, or others can't hear you at all**: check the microphone volume in the headset's mixer (read only; this is safe):
  ```sh
  amixer -c 0 cget name='VA_DEC0 Volume'
  amixer -c 0 cget name='VA_DEC1 Volume'
  ```
  Both should say `values=96`. If they say `0`, the microphone is effectively muted. This app never touches the mixer, but it has been seen dropping to 0 on its own. To set them back (at your own risk; note the current values and the `max=` shown by `cget` first, and use the value your headset normally has, 96 on ours):
  ```sh
  amixer -c 0 cset name='VA_DEC0 Volume' 96
  amixer -c 0 cset name='VA_DEC1 Volume' 96
  ```
- **No sound at all in SteamVR or Steam Link after PipeWire or WirePlumber was restarted**: restart SteamVR on the headset (or reconnect Steam Link). The sound comes back.
- **No sound at all after a SteamOS update**: the switch script is loaded as a required part of WirePlumber, so if an update breaks it, WirePlumber may stop and take all sound with it. Remove the config over SSH and restart the headset (or run `./install.sh --uninstall` and restart):
  ```sh
  rm ~/.config/wireplumber/wireplumber.conf.d/90-frame-mic.conf
  ```
- **"In use" while you're not talking in any app**: any app that records audio counts, including screen or audio recorders. SteamOS turns the filters on for all of them.
- **Frame Mic Tuner is missing from the + list**: restart the headset once after installing.
- **The Mic icon is missing from the dashboard although the app is running**: SteamVR can lose the app's dashboard panel (for example when the dashboard itself restarts). The app checks every 3 seconds and recreates the panel by itself, so wait a few seconds and open the dashboard again. Launching the app again from **+** also checks and recreates it before opening the panel. If it still doesn't come back, the log (`journalctl --user -u frame-mic-tuner -f` when it runs as a service) says why (the log is in Japanese; look for lines starting with "自己修復", which means "self-repair"); quitting the app and starting it again fixes it too.
- Logs: `journalctl --user -u frame-mic-tuner -f` when it runs as a service. To see the log of a manual run, quit the app and start `~/.local/bin/frame-mic-tuner` from SSH.
- **The update check fails, or "Update" doesn't do anything**: `~/.cache/frame-mic-tuner/update.log` has the details, and `~/.cache/frame-mic-tuner/update-check.json` / `update-state.json` the last raw answer. Turning **Check for updates** off (or `update_check: false` in the settings file) stops the automatic daily check without removing **Check now** / **Update**.
- **The settings didn't switch when I plugged in earphones**: the app only follows the *default* output. If SteamVR keeps playing through the Frame's speakers, press **Play sound here** on the earphones in **Output** (or choose them in SteamVR's audio settings). Outputs that aren't real devices (filters, virtual outputs) are ignored.
- **"Couldn't open Konsole"** when installing another app: Konsole needs Developer Mode (Steam Settings → System → Enable Developer Mode).

## Privacy

- Voice check recordings are kept only in the app's memory. They are never written to disk or to logs, and never sent anywhere. They are gone when the app quits.
- While it records, the panel shows "● Recording". Recording stops as soon as you close the panel.
- The microphone may pick up the voices of people around you. Keep that in mind before recording.
- To pick the default language it reads the `language` line of Steam's `~/.steam/registry.vdf` once at startup (read only).
- The app has no telemetry. Its network use is the update check and the app list (below):
  - **When**: while the app runs, it asks GitHub at startup if its last answer is more than 24 hours old, and after that about once a day (an hour later after a failed check). **Check now** asks right away. Turn **Check for updates** off in **Apps & updates** (or set `update_check: false` in the settings file) to stop the automatic check; then it only connects when you press **Check now** or **Update**.
  - **What it sends**: one HTTPS request for this project's latest release to `api.github.com`, with no account, ID or usage data. Like any web request, GitHub sees your IP address and the User-Agent of `curl`. Your recordings, settings and logs are never sent.
  - **What it receives**: the latest release's version number, its page and the names and links of its files. The answer is kept in `~/.cache/frame-mic-tuner/update-check.json`.
  - **Only when you press Update and confirm**, it also downloads that release's package and `SHA256SUMS` from `github.com` / `*.githubusercontent.com`.
- With **Check for updates** on, it also fetches the list of the author's other Steam Frame apps and their icons from `frame.sasaken1102s.net` (at most once an hour, shared with the author's other apps; kept in `~/.cache/frame-apps/`). It is a plain HTTPS GET with nothing attached (`curl -q --proto =https --max-redirs 0`): like GitHub, `frame.sasaken1102s.net` sees your IP address and the User-Agent of `curl`, and nothing else is sent. Turning **Check for updates** off stops this too, and the panel then shows the last fetched list or the one built into the app. When you install another app from **Apps & updates**, Konsole opens and runs the installer command shown on the confirmation, which connects to `frame.sasaken1102s.net` and GitHub inside Konsole.
- The only files it writes are `~/.config/frame-mic-tuner/config.json` (UI language, last tab, the update-check setting, and for each output you used its name, the Frame mic settings and the date it was last used), `~/.config/frame-mic-tuner/install-args` (the options your last `./install.sh` run used, so an update reinstalls the same way), `~/.cache/frame-mic-tuner/` (the update check's cached answer, the update's log and state, a lock folder `update.lock/` while an update runs, and a work folder whose download is deleted after each update), `~/.cache/frame-apps/` (the fetched app list and icons, shared with the author's other apps), and a lock file in `$XDG_RUNTIME_DIR` that holds its process ID (to stop a second copy from starting). An update also rewrites the installed files, as running `./install.sh` does.

## Disclaimer

- Use at your own risk. This project was made with Claude Opus 5.5, an AI model. I've tested it on my own Steam Frame, but I can't take responsibility for what happens on yours, so please read the code and check it yourself before you run it. The software comes with no warranty (see [LICENSE](LICENSE)).
- What it changes: the two WirePlumber settings `frame-mic.echo-cancel` and `frame-mic.noise-suppression` (through `wpctl settings`), the noise suppressor's two live parameters "VAD Threshold (%)" and "VAD Grace Period (ms)" (through `pw-cli set-param`; SteamOS puts its defaults back when its audio restarts), and WirePlumber's configuration in your home directory, where it replaces SteamOS's microphone tracker with its own script. When you press **Play sound here**, **Use this mic** / **Use the built-in mic** or **Mute** / **Unmute**, it changes the default output, the default mic or its mute (through `wpctl set-default` / `wpctl set-mute`, like SteamVR's audio settings do). Apart from that it only enables or disables its own systemd user unit when you ask it to, opens Konsole with the installer when you confirm installing another app, and, when you update from the panel, runs the new release's `install.sh` in a short-lived systemd user unit (`frame-mic-tuner-update`).
- It never restarts PipeWire or WirePlumber, never writes to the ALSA mixer (amixer), never touches `/etc` and never needs root.
- **Update**: downloads the latest release's `.tar.gz` and `SHA256SUMS` from GitHub over HTTPS (only to GitHub's own hosts), checks the hash, and only then extracts it into `~/.cache/frame-mic-tuner/update/` and runs its `install.sh` with the same options as your last install. It refuses a release with no `SHA256SUMS`, a hash mismatch, or an archive containing an absolute path, `..`, a link or a special file; nothing is changed if any of that happens. The download is deleted afterwards. `SHA256SUMS` is a checksum published in the same release, not a signature: it catches a corrupted or incomplete download, but not a release that was replaced on GitHub. The running app isn't restarted: quit it and start it again to use the new version. See [frame-update.sh's own notes](vendor/frame-updater/frame-update.sh) for the exact steps.
- A SteamOS update can change how the microphone filters are set up. If that happens, the switches may stop working until this project is updated. Because the switch script is a required part of WirePlumber, a script that no longer loads can also stop all sound on the headset (see Troubleshooting). `./install.sh --uninstall` and a restart bring back SteamOS's original behaviour.
- This is an unofficial project with no affiliation with or endorsement from Valve Corporation. Steam, Steam Frame, SteamOS, SteamVR and Steam Link are trademarks and/or registered trademarks of Valve Corporation in the U.S. and/or other countries. The names are used here only to say what this works with.

## Development

Build on the headset:

```sh
cmake -G Ninja -S . -B build && ninja -C build
./build/frame-mic-tuner --help
scripts/package.sh   # release build: dist/frame-mic-tuner-<version>.tar.gz, dist/SHA256SUMS
```

Useful options that don't need SteamVR: `--print`, `--set-ns-vad N` / `--set-ns-grace N` (apply a noise filter strength right away without saving it), `--dump-png PATH` (render the panel, with `--fake-*` states for screenshots, such as `--fake-output`, `--fake-switched`, `--fake-external-mic`, `--fake-update STATE`, and `--view` / `--overlay` for the other screens), `--test-record 3` (record 3 seconds and play them back), `--contrast-report` (WCAG contrast of every color pair) and `--version`. Design notes, how the switch script works and test results are in [docs/DEVELOPMENT.ja.md](docs/DEVELOPMENT.ja.md) (Japanese).

The update mechanism (`vendor/frame-updater/`), the shared UI parts (`vendor/frame-ui/`) and the list of the author's other apps (`vendor/frame-apps/`) are copied from shared, private repositories; don't edit the copies by hand — their `verify.sh` (run by `scripts/package.sh`) checks that they weren't.

### Releasing

`scripts/package.sh` runs the `verify.sh` of the three copies under `vendor/`, builds a Release binary, and writes `dist/frame-mic-tuner-<version>.tar.gz` and `dist/SHA256SUMS` (required for the panel's **Update** button to accept the release). It prints the command that creates the GitHub release (and its tag) with both files attached; write the release notes in a file outside the repository first:

```sh
gh release create v<version> dist/frame-mic-tuner-<version>.tar.gz dist/SHA256SUMS --title v<version> --notes-file <release-notes-file>
```

Don't make it a draft or a prerelease: the panel looks at GitHub's latest release, which skips both.

## License

MIT. See [LICENSE](LICENSE). The bundled OpenVR header (`third_party/openvr/`) is BSD-3-Clause by Valve Corporation. `vendor/frame-updater/`, `vendor/frame-ui/` and `vendor/frame-apps/` (including the other apps' icons built into the program) are copies of the author's own code and are MIT like the rest of this repository. Licenses of the bundled header and the libraries it uses are listed in [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md). Changes are listed in [CHANGELOG.md](CHANGELOG.md).
