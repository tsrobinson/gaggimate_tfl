#!/usr/bin/env bash
#
# Generate LVGL bitmap font files for the TfL screensaver.
#
# Usage:
#   ./scripts/generate_transit_fonts.sh                          # uses default Overpass fonts in assets/fonts/
#   ./scripts/generate_transit_fonts.sh path/to/bold.otf path/to/light.otf   # uses custom fonts
#
# Requirements: Node.js and npx (lv_font_conv is invoked via npx)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
FONT_DIR="$PROJECT_DIR/src/display/fonts"
ASSETS_DIR="$PROJECT_DIR/assets/fonts"

BOLD_FONT="${1:-$ASSETS_DIR/overpass-bold.otf}"
LIGHT_FONT="${2:-$ASSETS_DIR/overpass-light.otf}"

if [ ! -f "$BOLD_FONT" ]; then
    echo "Error: Bold font not found: $BOLD_FONT"
    exit 1
fi
if [ ! -f "$LIGHT_FONT" ]; then
    echo "Error: Light font not found: $LIGHT_FONT"
    exit 1
fi

RANGE="-r 0x20-0x7E -r 0xA0-0x017F -r 0x2013-0x2014 -r 0x2018-0x201A -r 0x201C-0x201E -r 0x2020-0x2022 -r 0x2026 -r 0x2039-0x203A -r 0x20AC"

echo "Generating transit fonts..."
echo "  Bold:  $BOLD_FONT"
echo "  Light: $LIGHT_FONT"

for SIZE in 14 18 20 34 40; do
    echo "  transit_font_${SIZE} (bold, ${SIZE}px)"
    npx lv_font_conv --size "$SIZE" --bpp 4 --format lvgl --no-compress --lv-include lvgl.h \
        --font "$BOLD_FONT" $RANGE \
        --lv-font-name "transit_font_${SIZE}" \
        -o "$FONT_DIR/transit_font_${SIZE}.c"
done

for SIZE in 14 16 18 20; do
    echo "  transit_font_light_${SIZE} (light, ${SIZE}px)"
    npx lv_font_conv --size "$SIZE" --bpp 4 --format lvgl --no-compress --lv-include lvgl.h \
        --font "$LIGHT_FONT" $RANGE \
        --lv-font-name "transit_font_light_${SIZE}" \
        -o "$FONT_DIR/transit_font_light_${SIZE}.c"
done

echo "Done. Generated 9 font files in $FONT_DIR"
