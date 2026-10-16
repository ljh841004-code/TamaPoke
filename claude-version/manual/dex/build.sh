#!/bin/sh
# 몬 도감 PDF 만들기: ./build.sh <dex.json> <그림 폴더> <펌 버전> [fontsource/files]
set -e
cd "$(dirname "$0")"
J=${1:?dex.json}; A=${2:?그림 폴더}; V=${3:?버전}
python3 gen_dex.py "$J" "$A" "$V" 0
if [ -n "$4" ]; then python3 ../tools/make_fonts.py "$4" --out dex/fonts dex/dex.html dex/cover_7.html dex/dex.css dex/order.py a6/주문서_A6.html; fi
CH=${CHROME:-$(ls -d /opt/pw-browsers/chromium-*/chrome-linux/chrome 2>/dev/null | head -1)}
sed 's|</body>|<script src="check.js"></script></body>|' dex.html > _check.html
BAD=$(timeout 120 "$CH" --headless --no-sandbox --disable-gpu --virtual-time-budget=10000 --dump-dom "file://$PWD/_check.html" 2>/dev/null | grep -o 'data-bad="[^"]*"')
rm -f _check.html
echo "넘침 검사: $BAD"
[ "$BAD" = 'data-bad=""' ] || { echo "넘치는 칸이 있어요"; exit 1; }
"$CH" --headless --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf="$PWD/다마포케_몬도감_A6_보기용.pdf" "file://$PWD/dex.html" 2>/dev/null
python3 gen_dex.py "$J" "$A" "$V" 3
"$CH" --headless --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf="$PWD/.dex_bleed.pdf" "file://$PWD/dex.html" 2>/dev/null
for s in 6 6.5 7 7.5 8 8.5 9; do
  "$CH" --headless --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf="$PWD/.cover_$s.pdf" "file://$PWD/cover_$s.html" 2>/dev/null
done
python3 make_print.py "$V"
N=$(ls 인쇄용_몬도감_본문_*쪽.pdf | sed 's/[^0-9]//g')
python3 order.py "$N" "$V"
"$CH" --headless --no-sandbox --disable-gpu --no-pdf-header-footer --print-to-pdf="$PWD/주문안내_몬도감_인쇄소.pdf" "file://$PWD/주문서_몬도감.html" 2>/dev/null
