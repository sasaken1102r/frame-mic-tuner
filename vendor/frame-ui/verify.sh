#!/bin/sh
# Checks this copy of frame-ui (vendor/frame-ui/ in an app). Run it from package.sh:
#   sh vendor/frame-ui/verify.sh
# 1. Every file still matches MANIFEST.sha256 (nobody edited the copy by hand).
# 2. If the frame-ui repository is found (FRAME_UI_SRC, or a frame-ui folder next
#    to the app), the copy is the same as the repository's files (it isn't behind).
#    FRAME_UI_ALLOW_DRIFT=1 turns a difference into a warning.
# --local-only does only step 1. Exit status 0 = fine, 1 = a problem (the message says which).
set -u

here=$(cd "$(dirname "$0")" && pwd)

# SHA-256 of a file with CR removed, so a CRLF checkout on Windows hashes like the LF original
hash() {
    tr -d '\r' <"$1" | sha256sum | cut -c1-64
}

if [ ! -f "$here/MANIFEST.sha256" ]; then
    echo "frame-ui: $here/MANIFEST.sha256 is missing" >&2
    exit 1
fi
status=0
files=
while read -r sum name; do
    [ -n "$name" ] || continue
    files="$files $name"
    if [ ! -f "$here/$name" ]; then
        echo "frame-ui: $here/$name is missing" >&2
        status=1
    elif [ "$(hash "$here/$name")" != "$sum" ]; then
        echo "frame-ui: $here/$name was edited in the app; change frame-ui and run its sync.sh instead" >&2
        status=1
    fi
done <"$here/MANIFEST.sha256"
if [ "$status" -ne 0 ] || [ "${1:-}" = --local-only ]; then
    exit "$status"
fi

src=${FRAME_UI_SRC:-$here/../../../frame-ui}
if [ ! -f "$src/sync.sh" ]; then
    echo "frame-ui: copy intact ($(sed -n 's/^version //p' "$here/UPSTREAM")); repository not found, not compared"
    exit 0
fi
for name in $files; do
    [ "$name" != UPSTREAM ] || continue
    if [ ! -f "$src/$name" ] || [ "$(hash "$src/$name")" != "$(hash "$here/$name")" ]; then
        echo "frame-ui: $name differs from $src (run $src/sync.sh on the app)" >&2
        status=1
    fi
done
if [ "$status" -ne 0 ] && [ "${FRAME_UI_ALLOW_DRIFT:-0}" = 1 ]; then
    echo "frame-ui: continuing anyway (FRAME_UI_ALLOW_DRIFT=1)" >&2
    exit 0
fi
[ "$status" -ne 0 ] || echo "frame-ui: copy intact and matches $src"
exit "$status"
