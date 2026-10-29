#!/usr/bin/env bash
# Install or update Frame Mic Tuner on a Steam Frame. Run it on the headset as the normal user (no sudo
# needed), either from a downloaded release (extract the tar.gz first) or from a cloned repository
# (this builds it):
#   ./install.sh                     build if needed, then install or update
#   ./install.sh --autostart         same, and also start it together with SteamVR from now on
#   ./install.sh --uninstall         remove the app and the WirePlumber script (reboot afterwards)
#   ./install.sh --uninstall --purge also delete the app's settings and the saved frame-mic.* values
#
# This script never restarts PipeWire or WirePlumber (doing that while playing leaves SteamVR and
# Steam Link without sound), and never touches amixer, /etc or root. A new WirePlumber script takes
# effect after the next reboot.
set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
config_home="${XDG_CONFIG_HOME:-$HOME/.config}"
data_home="${XDG_DATA_HOME:-$HOME/.local/share}"
bin="$HOME/.local/bin/frame-mic-tuner"
desktop="$data_home/applications/frame-mic-tuner.desktop"
icons="$data_home/icons/hicolor"
unit_dir="$config_home/systemd/user"
unit="frame-mic-tuner.service"
app_config="$config_home/frame-mic-tuner"
wp_script="$data_home/wireplumber/scripts/frame-mic-tracker.lua"
wp_conf="$config_home/wireplumber/wireplumber.conf.d/90-frame-mic.conf"
update_script="$data_home/frame-mic-tuner/frame-update.sh"

# A release tar.gz has the built binary next to this script and no CMakeLists.txt; a git checkout has
# the source and no binary here yet (or an outdated one from a previous build).
if [[ -f "$here/CMakeLists.txt" ]]; then
    from_source=1
else
    from_source=0
fi

usage() {
    sed -n '2,7p' "$0" | sed 's/^# \{0,1\}//'
}

autostart=0
uninstall=0
purge=0
for arg in "$@"; do
    case "$arg" in
        --autostart) autostart=1 ;;
        --uninstall) uninstall=1 ;;
        --purge) purge=1 ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
    esac
done
if [[ $purge -eq 1 && $uninstall -eq 0 ]]; then
    echo "--purge only works together with --uninstall." >&2
    exit 2
fi

if [[ $uninstall -eq 1 ]]; then
    # Stop and disable the service, and quit an instance started from the dashboard (+), if any.
    systemctl --user disable --now "$unit" 2>/dev/null || true
    pkill -TERM -x frame-mic-tuner 2>/dev/null || true
    rm -f "$bin" "$desktop" "$unit_dir/$unit" "$wp_script" "$wp_conf" "$update_script"
    rmdir "$(dirname "$update_script")" 2>/dev/null || true
    for size in 48 128 256; do
        rm -f "$icons/${size}x${size}/apps/frame-mic-tuner.png"
    done
    systemctl --user daemon-reload
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database "$(dirname "$desktop")" || true
    fi
    if [[ $purge -eq 1 ]]; then
        rm -rf "$app_config"
        # Forget the saved switch values. Only these two keys; nothing else in WirePlumber changes.
        wpctl settings --delete frame-mic.echo-cancel >/dev/null 2>&1 || true
        wpctl settings --delete frame-mic.noise-suppression >/dev/null 2>&1 || true
        echo "Removed Frame Mic Tuner, its settings and the saved frame-mic.* values."
    else
        echo "Removed Frame Mic Tuner. Its settings are kept in $app_config (add --purge to delete them)."
    fi
    cat <<EOF

The WirePlumber script is removed but still loaded until the next start of WirePlumber.
Restart the headset when you're not playing: after that, Valve's original microphone behaviour is back
(echo cancellation, and noise suppression with the Frame's speakers, on while an app uses the mic).
Don't restart PipeWire or WirePlumber by hand while playing: SteamVR and Steam Link lose their sound
(restarting SteamVR brings it back).
EOF
    exit 0
fi

if [[ "$(uname -m)" != "aarch64" ]]; then
    echo "This is for the Steam Frame (aarch64), but this machine is $(uname -m)." >&2
    exit 1
fi
tools="wpctl systemctl"
if [[ $from_source -eq 1 ]]; then
    tools="cmake ninja g++ pkg-config $tools"
fi
for tool in $tools; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing tool: $tool" >&2
        exit 1
    fi
done

cd "$here"
if [[ $from_source -eq 1 ]]; then
    # Build (or bring an existing build up to date after a git pull)
    if [[ ! -f build/build.ninja ]]; then
        cmake -G Ninja -S . -B build -DBUILD_TESTING=OFF
    fi
    ninja -C build
    built_bin="build/frame-mic-tuner"
else
    # A release tar.gz: the binary is already built, right next to this script
    built_bin="$here/frame-mic-tuner"
fi

# The app: binary, launcher entry for the dashboard's "+" list, icons, and the systemd unit
install -Dm755 "$built_bin" "$bin"
# The shared update script (vendor/frame-updater), so the panel's update card can check and install updates
install -Dm755 vendor/frame-updater/frame-update.sh "$update_script"
for size in 48 128 256; do
    install -Dm644 "contrib/icons/frame-mic-tuner-$size.png" "$icons/${size}x${size}/apps/frame-mic-tuner.png"
done
mkdir -p "$(dirname "$desktop")"
sed "s|@BINARY@|$bin|" contrib/frame-mic-tuner.desktop > "$desktop.tmp"
chmod 644 "$desktop.tmp"
mv "$desktop.tmp" "$desktop"
install -Dm644 contrib/frame-mic-tuner.service "$unit_dir/$unit"
systemctl --user daemon-reload
if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "$desktop" || true
fi
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "$(dirname "$desktop")" || true
fi
if command -v gtk-update-icon-cache >/dev/null 2>&1 && [[ -f "$icons/index.theme" ]]; then
    gtk-update-icon-cache -q "$icons" || true
fi

# The WirePlumber script that lets the app switch the filters. Only the user's own folders are used
# (Valve's files in /etc stay as they are). A file is left alone when it already has the same content;
# the "SPDX-License-Identifier" first line is ignored in that comparison.
wp_changed=0
tmp="$(mktemp)"
trap 'rm -f "$tmp"' EXIT

# Print a file without its SPDX first line.
strip_spdx() {
    sed '1{/SPDX-License-Identifier/d}' "$1"
}

# Install $1 (already rendered) as $2 unless $2 has the same content.
install_if_changed() {
    local source="$1" target="$2"
    if [[ -f "$target" ]] && cmp -s <(strip_spdx "$source") <(strip_spdx "$target"); then
        echo "WirePlumber: $target is already up to date."
        return
    fi
    install -Dm644 "$source" "$target"
    echo "WirePlumber: installed $target"
    wp_changed=1
}

cp contrib/wireplumber/frame-mic-tracker.lua "$tmp"
install_if_changed "$tmp" "$wp_script"
sed "s|@SCRIPT_PATH@|$wp_script|" contrib/wireplumber/90-frame-mic.conf > "$tmp"
install_if_changed "$tmp" "$wp_conf"

if [[ $autostart -eq 1 ]]; then
    systemctl --user enable "$unit"
fi
autostart_state="$(systemctl --user is-enabled "$unit" 2>/dev/null || true)"

# Remember this run's options, so an update started from the panel (which runs this same script with no
# terminal to ask) reinstalls with the same options. Only --autostart matters here.
mkdir -p "$app_config"
: > "$app_config/install-args"
if [[ $autostart -eq 1 ]]; then
    echo --autostart >> "$app_config/install-args"
fi

cat <<EOF

Frame Mic Tuner is installed:
  $bin
  $desktop
  $unit_dir/$unit (start with SteamVR: ${autostart_state:-unknown}; switch it in the panel)
Open it from the SteamVR dashboard: + (launch a program) > Frame Mic Tuner, then the "Mic" icon.
EOF
if pgrep -x frame-mic-tuner >/dev/null 2>&1; then
    echo "A running Frame Mic Tuner keeps the old version until you quit it (dashboard > Mic > Quit) or restart SteamVR."
fi
if [[ $wp_changed -eq 1 ]]; then
    cat <<EOF

The WirePlumber script was installed or changed. Restart the headset to turn it on.
Don't restart PipeWire or WirePlumber while playing: SteamVR and Steam Link lose their sound
(restarting SteamVR brings it back).
EOF
fi
