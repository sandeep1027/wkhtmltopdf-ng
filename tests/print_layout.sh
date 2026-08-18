#!/bin/bash
set -u

# Viewport / print-background fixture. The HTML uses @media screen and
# (max-width: 700px) to stack table-cell columns. A 0-width WebEngine
# viewport would match that query and produce a mobile PDF.
# Usage: print_layout.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required for print-layout checks"
    echo "$failures print-layout check(s) failed"
    exit "$failures"
fi

pdf="$out_dir/print-layout.pdf"
html="$data_dir/print-layout.html"
if ! "$bin" --enable-local-file-access --javascript-delay 200 \
        "$html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "print-layout PDF renders"
    echo "1 print-layout check(s) failed"
    exit 1
fi
check 0 "print-layout PDF renders"

png="$out_dir/print-layout.png"
gs -q -dNOPAUSE -dBATCH -sDEVICE=png16m -r72 -dFirstPage=1 -dLastPage=1 \
    -sOutputFile="$png" "$pdf" >/dev/null 2>&1
check $? "rasterize first page"

python3 - "$png" "$pdf" <<'PY'
import re, struct, subprocess, sys, zlib

png_path, pdf_path = sys.argv[1], sys.argv[2]
failures = 0

def fail(msg):
    global failures
    print("FAIL:", msg)
    failures += 1

def ok(msg):
    print("PASS:", msg)

# --- layout: KPI labels must share a line (not stacked) ---
text = subprocess.check_output(
    ["gs", "-q", "-dNOPAUSE", "-dBATCH", "-sDEVICE=txtwrite",
     "-sOutputFile=-", pdf_path], stderr=subprocess.DEVNULL)
text = text.decode("utf-8", "replace")
compact_lines = [re.sub(r"\s+", " ", line).strip() for line in text.splitlines() if line.strip()]
joined = " | ".join(compact_lines)
if any("Revenue" in line and "Customers" in line and "Conversion" in line and "Retention" in line
       for line in compact_lines):
    ok("KPI cards are on one line")
else:
    fail("KPI cards are stacked or missing: %r" % compact_lines[:12])

if any("Executive Overview left" in line and "Executive Overview right" in line
       for line in compact_lines):
    ok("overview columns are on one line")
else:
    fail("overview columns are stacked: %r" % compact_lines[:12])

if any("Priority A" in line and "Priority B" in line and "Priority C" in line
       for line in compact_lines):
    ok("feature cards are on one line")
else:
    fail("feature cards are stacked: %r" % compact_lines[:12])

# --- header gradient: top band must contain saturated blue/purple pixels ---
def read_png(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
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
    for _ in range(height):
        filt = raw[i]
        scan = bytearray(raw[i+1:i+1+stride])
        i += 1 + stride
        if filt == 1:
            for x in range(stride):
                left = scan[x-3] if x >= 3 else 0
                scan[x] = (scan[x] + left) & 255
        elif filt == 2:
            for x in range(stride):
                scan[x] = (scan[x] + prev[x]) & 255
        elif filt == 3:
            for x in range(stride):
                left = scan[x-3] if x >= 3 else 0
                scan[x] = (scan[x] + ((left + prev[x]) // 2)) & 255
        elif filt == 4:
            def paeth(a, b, c):
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                if pa <= pb and pa <= pc:
                    return a
                if pb <= pc:
                    return b
                return c
            for x in range(stride):
                left = scan[x-3] if x >= 3 else 0
                up = prev[x]
                ul = prev[x-3] if x >= 3 else 0
                scan[x] = (scan[x] + paeth(left, up, ul)) & 255
        rows.append(bytes(scan))
        prev = scan
    return width, height, rows

width, height, rows = read_png(png_path)
header_end = max(8, height // 8)
colorful = 0
samples = 0
for y in range(4, header_end):
    row = rows[y]
    for x in range(8, width - 8, 4):
        r, g, b = row[x*3], row[x*3+1], row[x*3+2]
        samples += 1
        if b > 120 and r < 180 and (b > r + 20 or g < 160):
            colorful += 1
if colorful > samples * 0.15:
    ok("header has a colored gradient (%d/%d pixels)" % (colorful, samples))
else:
    fail("header looks unpainted (%d/%d colorful pixels)" % (colorful, samples))

# progress bar sits just below the KPI row (still in the top third)
bar_colorful = 0
for y in range(header_end, min(height, header_end + 200)):
    row = rows[y]
    for x in range(8, width - 8, 2):
        r, g, b = row[x*3], row[x*3+1], row[x*3+2]
        if b > 180 and r < 160 and g < 180:
            bar_colorful += 1
if bar_colorful > 20:
    ok("progress bar has a colored fill (%d pixels)" % bar_colorful)
else:
    fail("progress bar fill missing (%d colorful pixels)" % bar_colorful)

sys.exit(failures)
PY
check $? "layout and background pixels"

if [ "$failures" -eq 0 ]; then
    echo "ALL PRINT-LAYOUT CHECKS PASSED"
else
    echo "$failures print-layout check(s) failed"
fi
exit "$failures"
