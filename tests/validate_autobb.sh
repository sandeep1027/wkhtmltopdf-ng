#!/bin/bash
set -u

# Auto page-break must not put body text under a CSS position:fixed footer.
# Usage: validate_autobb.sh BINARY TEST_DATA_DIR OUTPUT_DIR

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

bin=${1:?binary required}
data_dir=${2:?test data dir required}
out_dir=${3:?output dir required}
mkdir -p "$out_dir"
failures=0

check() {
    if [ "$1" -eq 0 ]; then
        echo "PASS: $2"
    else
        echo "FAIL: $2"
        failures=$((failures + 1))
    fi
}

pdf="$out_dir/validate-autobb.pdf"
if ! "$bin" --javascript-delay 0 --window-status flow-ready --compress \
        "$data_dir/validate-autobb.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "auto-break PDF renders"
    echo "1 auto-break check(s) failed"
    exit 1
fi
check 0 "auto-break PDF renders"

pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
if [ "${pages:-0}" -ge 2 ]; then
    check 0 "auto-break spans 2+ pages ($pages)"
else
    check 1 "auto-break spans 2+ pages (got ${pages:-none})"
fi

p1=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -dFirstPage=1 -dLastPage=1 \
    -sOutputFile=- "$pdf" 2>/dev/null | tr -s ' \n' ' ')
p2=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -dFirstPage=2 -dLastPage=2 \
    -sOutputFile=- "$pdf" 2>/dev/null | tr -s ' \n' ' ')

case "$p1" in *AUTO-FOOTER*) check 0 "page 1 has footer" ;;
    *) check 1 "page 1 has footer" ;; esac
case "$p2" in *AUTO-HEADER*) check 0 "page 2 has header" ;;
    *) check 1 "page 2 has header" ;; esac

# Paint order can list a LINE- after AUTO-FOOTER even when the footer
# covers it. Check the bottom strip of the raster: body ink (#111) must
# not sit in the red footer band.
png="$out_dir/validate-autobb-p1.png"
gs -q -dNOPAUSE -dBATCH -sDEVICE=png16m -r72 -dFirstPage=1 -dLastPage=1 \
    -sOutputFile="$png" "$pdf"
python3 - "$png" <<'PY'
import struct, sys, zlib
path = sys.argv[1]
data = open(path, "rb").read()
pos = 8
width = height = None
idat = b""
while pos + 8 <= len(data):
    length = struct.unpack(">I", data[pos:pos+4])[0]
    ctype = data[pos+4:pos+8]
    chunk = data[pos+8:pos+8+length]
    pos += 12 + length
    if ctype == b"IHDR":
        width, height = struct.unpack(">II", chunk[:8])
    elif ctype == b"IDAT":
        idat += chunk
    elif ctype == b"IEND":
        break
raw = zlib.decompress(idat)
stride = width * 3
rows = []
i = 0
prev = bytearray(stride)
def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    return a if pa <= pb and pa <= pc else (b if pb <= pc else c)
for _ in range(height):
    filt = raw[i]
    scan = bytearray(raw[i+1:i+1+stride])
    i += 1 + stride
    if filt == 1:
        for x in range(stride):
            scan[x] = (scan[x] + (scan[x-3] if x >= 3 else 0)) & 255
    elif filt == 2:
        for x in range(stride):
            scan[x] = (scan[x] + prev[x]) & 255
    elif filt == 4:
        for x in range(stride):
            left = scan[x-3] if x >= 3 else 0
            up = prev[x]
            ul = prev[x-3] if x >= 3 else 0
            scan[x] = (scan[x] + paeth(left, up, ul)) & 255
    rows.append(bytes(scan))
    prev = scan
# Footer is the last ~14mm of A4 (~4.7%). Sample the last 5%.
band = rows[int(height * 0.95):]
ink = 0
for row in band:
    for x in range(0, len(row), 3):
        r, g, b = row[x], row[x+1], row[x+2]
        # dark body text, not red footer / white / blue header
        if r < 80 and g < 80 and b < 80:
            ink += 1
# allow a few anti-alias pixels
sys.exit(0 if ink < 40 else 1)
PY
if [ $? -eq 0 ]; then
    check 0 "page 1 footer band has no body text"
else
    check 1 "page 1 footer band has no body text"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL AUTO-BREAK FOOTER CHECKS PASSED"
else
    echo "$failures auto-break footer check(s) failed"
fi
exit "$failures"
