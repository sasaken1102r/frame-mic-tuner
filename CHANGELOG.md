# Changelog

## Unreleased

- Mute indicator: the panel notices when the default mic is muted, however it was muted (Steam, `wpctl set-mute`, an aux button shortcut, ...), by reading `wpctl get-volume @DEFAULT_AUDIO_SOURCE@` with the rest of the state. While muted, the header badge turns into a red "Muted" with a crossed-out mic, and a red "Mic is muted" band with an **Unmute** button replaces the update card on both tabs (the update card comes back once unmuted). **Unmute** runs `wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 0` without asking first; if that fails, the bottom line says so. Nothing looks different while the mic is not muted.

## 0.2.0 (2026-09-27)

First release with a prebuilt package (`frame-mic-tuner-<version>.tar.gz` + `SHA256SUMS`, built by `scripts/package.sh`); `install.sh` now installs either from that tarball or, as before, by building from a source checkout.

- Updates: an update card above the language/autostart/quit row always shows the running version. It asks GitHub for a newer release about once a day (`update_check` setting, on by default), and **Check now** looks right away regardless, even with automatic checking off. When a newer version is available, **Update** downloads and installs it after you confirm once (the card asks again with **Cancel** / **Update**; checksum-verified against the release's `SHA256SUMS`; nothing changes if that check fails). Shared with the other Frame apps via `vendor/frame-updater/`.
- The update card shows its state at a glance: an accent-colored border for a new version, the confirmation and a finished install, a red border with **Try again** / **Close** when an update failed, and red text (with **Check now**) when only the check failed.
- `install.sh` now writes `~/.config/frame-mic-tuner/install-args` with the options your last install used (currently just `--autostart`, when passed), so an update started from the panel reinstalls the same way.

## 0.1.0 (2026-09-27)

First release.

- A "Mic" panel in the SteamVR dashboard that switches the Steam Frame's microphone filters, with two tabs ("Quick" and "Fine-tune"; it reopens on the last one): presets for how you listen ("Earphones / headphones" = echo cancellation off and noise suppression off, "Frame speakers" = echo cancellation on and noise suppression off; each card shows what it sets, and both cards turn grey when the settings match neither), plus separate "Fine-tune" switches for echo cancellation and noise suppression. Changes apply at once, even while the mic is in use, and are kept after a restart.
- Noise filter strength sliders (Strictness = VAD threshold, Hold = VAD grace period) that apply live without restarting PipeWire. The values are saved and applied again whenever the app starts.
- Shows whether the mic is in use and which filters the sound actually goes through, read from the live PipeWire links.
- Voice check: record up to 10 seconds of what apps hear, keep the last 5 recordings in memory and play them back to compare settings. Nothing is written to disk.
- Japanese and English UI. It starts in the Steam Frame's language (from Steam's language setting) and remembers your choice. Colors meet WCAG 2.x AA contrast (`--contrast-report` prints every pair).
- Self-repair: if SteamVR loses the app's dashboard panel (for example when the dashboard restarts), the app notices within 3 seconds and recreates it.
- Starts from the dashboard's "+" (launch a program) list. Launching it again while it runs opens its panel. Optional autostart with SteamVR, switched from the panel.
- Includes the WirePlumber script (`contrib/wireplumber/`) that makes the filters switchable without restarting PipeWire.
- `install.sh` builds and installs everything into the home directory, with no sudo and no PipeWire / WirePlumber restart.
