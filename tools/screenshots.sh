#!/usr/bin/env bash
# App store screenshots: builds the face in demo mode (MASL_DEMO in
# src/c/main.c), then on each platform's emulator takes one screenshot per
# demo scene, tapping to move between them. Finishes with a normal build so
# the .pbw in build/ is safe to release.
#
# Needs the Pebble SDK (the `pebble` command). Run from anywhere:
#   tools/screenshots.sh                 every platform
#   tools/screenshots.sh basalt chalk    just these
#
# Pictures land in screenshots/<platform>/<n>-<scene>.png.

set -euo pipefail

cd "$(dirname "$0")/.."

# Keep in sync with the demo scenes in src/c/main.c, in the same order.
SCENES=(team live-split league final-custom)

if [ $# -gt 0 ]; then
  PLATFORMS=("$@")
else
  PLATFORMS=(basalt chalk diorite emery flint gabbro)
fi

# Seconds to let the face draw after it launches and after each tap.
SETTLE=${SETTLE:-2}

restore_release_build() {
  echo "Rebuilding without demo mode..."
  pebble build >/dev/null
}

echo "Building in demo mode..."
MASL_DEMO=1 pebble build
trap restore_release_build EXIT

for p in "${PLATFORMS[@]}"; do
  echo "== $p"
  out="screenshots/$p"
  mkdir -p "$out"

  pebble install --emulator "$p"
  pebble emu-time-format --emulator "$p" --format 12h
  pebble emu-battery --emulator "$p" --percent 80
  pebble emu-bt-connection --emulator "$p" --connected yes
  # The settings above can restart the face; start it again so it's on the
  # first scene.
  pebble install --emulator "$p"
  sleep "$SETTLE"

  for i in "${!SCENES[@]}"; do
    file="$out/$((i + 1))-${SCENES[$i]}.png"
    pebble screenshot --no-open --emulator "$p" "$file"
    echo "   $file"
    pebble emu-tap --emulator "$p"
    sleep "$SETTLE"
  done
done

pebble kill || true
echo "Done. Screenshots are in screenshots/."
