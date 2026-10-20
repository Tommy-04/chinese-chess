#!/usr/bin/env bash
set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
FONT_DIR="$ROOT_DIR/lvgl/scripts/built_in_font"
OUT_FILE="$ROOT_DIR/lvgl/src/font/lv_font_simsun_16_cjk.c"
TMP_FILE="$OUT_FILE.tmp"

if ! command -v lv_font_conv >/dev/null 2>&1; then
    echo "lv_font_conv 未安装，请先执行：sudo npm install -g lv_font_conv" >&2
    exit 1
fi

symbols="$(python3 - "$ROOT_DIR/src" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])
chars = set()
for path in root.rglob('*'):
    if path.suffix not in {'.c', '.h'}:
        continue
    try:
        text = path.read_text(encoding='utf-8')
    except UnicodeDecodeError:
        continue
    chars.update(
        char for char in text
        if 0x80 <= ord(char) <= 0xffff
        and not char.isspace()
        and not 0xe000 <= ord(char) <= 0xf8ff
    )
print(''.join(sorted(chars)), end='')
PY
)"

fontawesome_symbols="61441,61448,61451,61452,61452,61453,61457,61459,61461,61465,61468,61473,61478,61479,61480,61502,61507,61512,61515,61516,61517,61521,61522,61523,61524,61543,61544,61550,61552,61553,61556,61559,61560,61561,61563,61587,61589,61636,61637,61639,61641,61664,61671,61674,61683,61724,61732,61787,61931,62016,62017,62018,62019,62020,62087,62099,62212,62189,62810,63426,63650"

lv_font_conv \
    --no-compress --no-prefilter \
    --bpp 4 --size 16 \
    --font "$FONT_DIR/SimSun.woff" \
    -r 0x20-0x7F \
    --symbols "$symbols" \
    --font "$FONT_DIR/FontAwesome5-Solid+Brands+Regular.woff" \
    -r "$fontawesome_symbols" \
    --format lvgl \
    --lv-font-name lv_font_simsun_16_cjk \
    -o "$TMP_FILE" \
    --force-fast-kern-format

mv "$TMP_FILE" "$OUT_FILE"
echo "字体已生成：$OUT_FILE"
