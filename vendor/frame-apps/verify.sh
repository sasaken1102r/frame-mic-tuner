#!/bin/sh
# SPDX-License-Identifier: MIT — part of frame-apps by sasaken1102r, shipped under the host app's MIT license
# Checks this copy of frame-apps (vendor/frame-apps/ in an app). Run it from package.sh:
#   sh vendor/frame-apps/verify.sh
# 1. Every file still matches MANIFEST.sha256 (nobody edited the copy by hand).
# 2. If the frame-apps repository is found (FRAME_APPS_SRC, or a frame-apps folder next
#    to the app), the copy is the same as the repository's files (it isn't behind).
#    FRAME_APPS_ALLOW_DRIFT=1 turns a difference into a warning.
# --local-only does only step 1. Exit status 0 = fine, 1 = a problem (the message says which).
set -u

here=$(cd "$(dirname "$0")" && pwd)

# SHA-256 of a file with CR removed, so a CRLF checkout on Windows hashes like the LF original
hash() {
    tr -d '\r' <"$1" | sha256sum | cut -c1-64
}

if [ ! -f "$here/MANIFEST.sha256" ]; then
    echo "frame-apps: $here/MANIFEST.sha256 is missing" >&2
    exit 1
fi
status=0
files=
while read -r sum name; do
    [ -n "$name" ] || continue
    files="$files $name"
    if [ ! -f "$here/$name" ]; then
        echo "frame-apps: $here/$name is missing" >&2
        status=1
    elif [ "$(hash "$here/$name")" != "$sum" ]; then
        echo "frame-apps: $here/$name was edited in the app; change frame-apps and run its sync.sh instead" >&2
        status=1
    fi
done <"$here/MANIFEST.sha256"
if [ "$status" -ne 0 ] || [ "${1:-}" = --local-only ]; then
    exit "$status"
fi

src=${FRAME_APPS_SRC:-$here/../../../frame-apps}
if [ ! -f "$src/sync.sh" ]; then
    echo "frame-apps: copy intact ($(sed -n 's/^version //p' "$here/UPSTREAM")); repository not found, not compared"
    exit 0
fi
for name in $files; do
    [ "$name" != UPSTREAM ] || continue
    if [ ! -f "$src/$name" ] || [ "$(hash "$src/$name")" != "$(hash "$here/$name")" ]; then
        echo "frame-apps: $name differs from $src (run $src/sync.sh on the app)" >&2
        status=1
    fi
done
if [ "$status" -ne 0 ] && [ "${FRAME_APPS_ALLOW_DRIFT:-0}" = 1 ]; then
    echo "frame-apps: continuing anyway (FRAME_APPS_ALLOW_DRIFT=1)" >&2
    exit 0
fi
[ "$status" -ne 0 ] || echo "frame-apps: copy intact and matches $src"
exit "$status"
