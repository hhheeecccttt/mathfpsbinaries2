#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Rebuild the game with the current map export.
#
# The 2fort map is data, not code: assets/2fort.obj is read from disk at
# startup, so "adding a new version of the map" just means dropping the new
# export into assets/ and running the build, which copies the binary, assets/
# and shaders/ into the runtime folder.
#
#   ./rebuild_map.sh              rebuild if the map, an asset, a shader or any
#                                 source file changed since the last build
#   ./rebuild_map.sh --force      rebuild no matter what
#   ./rebuild_map.sh --watch      keep running and rebuild the moment a new
#                                 version of the map lands in assets/
#
# Environment:
#   OUT=<dir>    runtime folder         (default ../mathfpsbinaries2)
#   MAP=<file>   map to watch           (default assets/2fort.obj)
#   POLL=<secs>  --watch poll interval  (default 2)
# ---------------------------------------------------------------------------
set -euo pipefail

cd "$(dirname "$0")"

OUT="${OUT:-../mathfpsbinaries2}"
MAP="${MAP:-assets/2fort.obj}"
STAMP=".rebuild.stamp"
POLL="${POLL:-2}"

# Everything whose change should trigger a rebuild. Content hash, so simply
# re-saving (touching) a file does not count as a new version.
inputs_hash() {
    {
        find assets shaders -type f -print0 | sort -z | xargs -0 md5sum
        md5sum ./*.cpp ./*.h 2>/dev/null
    } | md5sum | cut -d' ' -f1
}

check_map() {
    [ -f "$MAP" ] || { echo "[map] ERROR: $MAP not found" >&2; exit 1; }
    local pieces faces nonquad
    pieces=$(grep -c '^o ' "$MAP" || true)
    faces=$(grep -c '^f ' "$MAP" || true)
    # The loader gives every quad its own copy of the texture and fits an
    # oriented collider to box-shaped pieces; both assume quad faces.
    nonquad=$(awk '/^f /{ if (NF-1 != 4) c++ } END { print c+0 }' "$MAP")
    echo "[map] $MAP: $pieces pieces, $faces faces"
    if [ "$nonquad" -ne 0 ]; then
        echo "[map] WARNING: $nonquad non-quad face(s); the loader expects quads" >&2
    fi
}

build() {
    # A running game keeps the binary busy ("Text file busy" when copying over it).
    if pkill -x dogtags 2>/dev/null; then
        echo "[map] stopped the running instance"
        sleep 0.3
    fi
    echo "[map] building..."
    ./build_linux.sh
    inputs_hash > "$STAMP"
    echo "[map] done $(date '+%H:%M:%S') -> $OUT/dogtags"
}

force=0
watch=0
case "${1:-auto}" in
    --force|-f) force=1 ;;
    --watch|-w) watch=1 ;;
esac

check_map
current=$(inputs_hash)

if [ "$watch" = 1 ]; then
    echo "[map] watching for a new export (poll every ${POLL}s). Ctrl-C to stop."
    if [ "$current" != "$(cat "$STAMP" 2>/dev/null || true)" ] || [ ! -x "$OUT/dogtags" ]; then
        build
    fi
    while true; do
        sleep "$POLL"
        current=$(inputs_hash)
        if [ "$current" != "$(cat "$STAMP" 2>/dev/null || true)" ]; then
            echo "[map] new version detected $(date '+%H:%M:%S')"
            build
        fi
    done
else
    if [ "$force" = 1 ] || [ "$current" != "$(cat "$STAMP" 2>/dev/null || true)" ] \
       || [ ! -x "$OUT/dogtags" ]; then
        build
    else
        echo "[map] up to date; use --force to rebuild anyway"
    fi
fi
