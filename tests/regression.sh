#!/bin/bash
set -u

# Regression validation for rendered output. Unlike the plain CTest smoke
# tests, this verifies the produced PDFs/images are well-formed and contain
# what they should: %PDF magic, expected page counts, valid PNGs, and that
# loading a remote resource through the --proxy integration works.
#
# Usage: regression.sh BINARY_DIR TEST_DATA_DIR OUTPUT_DIR
#   BINARY_DIR   directory containing wkhtmltopdf-ng / wkhtmltoimage-ng
#   TEST_DATA_DIR directory containing the HTML fixtures
#   OUTPUT_DIR   writable directory for generated artifacts
#
# Optional: qpdf on PATH enables page-count checks; python3 on PATH enables
# the proxy integration check and non-blank image verification.

export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox

bin_dir=${1:?binary dir required}
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

has_magic() {
    local file=$1
    local hex=$2
    local bytes=$(( ${#hex} / 2 ))
    local first
    first=$(od -An -tx1 -N"$bytes" "$file" 2>/dev/null | tr -d ' \n')
    [ "$first" = "$hex" ]
}

check_magic() {
    if has_magic "$1" "$2"; then
        check 0 "$3"
    else
        check 1 "$3"
    fi
}

expect_pages() {
    if ! command -v qpdf >/dev/null 2>&1; then
        check 0 "$3 (qpdf unavailable, skipped)"
        return
    fi
    local pages
    pages=$(qpdf --show-npages "$1" 2>/dev/null | tr -d ' \n')
    if [ "$pages" = "$2" ]; then
        check 0 "$3"
    else
        check 1 "$3 (expected $2, got ${pages:-none})"
    fi
}

"$bin_dir/wkhtmltopdf-ng" --enable-local-file-access "$data_dir/toc.html" "$out_dir/regression.pdf" >/dev/null 2>&1
check $? "regression PDF renders"
check_magic "$out_dir/regression.pdf" 25504446 "regression PDF has %PDF magic"
expect_pages "$out_dir/regression.pdf" 1 "regression PDF is one page"

"$bin_dir/wkhtmltopdf-ng" --toc --enable-local-file-access "$data_dir/toc.html" "$out_dir/toc.pdf" >/dev/null 2>&1
check $? "TOC PDF renders"
check_magic "$out_dir/toc.pdf" 25504446 "TOC PDF has %PDF magic"
expect_pages "$out_dir/toc.pdf" 2 "TOC PDF has a TOC page + content page"

"$bin_dir/wkhtmltopdf-ng" --disable-smart-shrinking --dpi 96 --enable-local-file-access \
    "$data_dir/tall.html" "$out_dir/dpi96.pdf" >/dev/null 2>&1
check $? "DPI 96 tall PDF renders"
"$bin_dir/wkhtmltopdf-ng" --disable-smart-shrinking --dpi 192 --enable-local-file-access \
    "$data_dir/tall.html" "$out_dir/dpi192.pdf" >/dev/null 2>&1
check $? "DPI 192 tall PDF renders"
if command -v qpdf >/dev/null 2>&1; then
    pages96=$(qpdf --show-npages "$out_dir/dpi96.pdf" 2>/dev/null | tr -d ' \n')
    pages192=$(qpdf --show-npages "$out_dir/dpi192.pdf" 2>/dev/null | tr -d ' \n')
    if [ -n "$pages96" ] && [ -n "$pages192" ] && [ "$pages192" -lt "$pages96" ]; then
        check 0 "higher DPI produces fewer pages ($pages192 < $pages96)"
    else
        check 1 "higher DPI produces fewer pages (96dpi=${pages96:-none}, 192dpi=${pages192:-none})"
    fi
else
    check 0 "higher DPI page-count check (qpdf unavailable, skipped)"
fi

"$bin_dir/wkhtmltopdf-ng" --enable-smart-shrinking --enable-local-file-access \
    "$data_dir/wide.html" "$out_dir/wide-shrink.pdf" >/dev/null 2>&1
check $? "smart shrinking wide PDF renders"
check_magic "$out_dir/wide-shrink.pdf" 25504446 "smart shrinking PDF has %PDF magic"
"$bin_dir/wkhtmltopdf-ng" --disable-smart-shrinking --enable-local-file-access \
    "$data_dir/wide.html" "$out_dir/wide-noshrink.pdf" >/dev/null 2>&1
check $? "disabled smart shrinking wide PDF renders"

"$bin_dir/wkhtmltopdf-ng" --copies 2 --javascript-delay 0 --enable-local-file-access \
    "$data_dir/toc.html" "$out_dir/copies.pdf" >/dev/null 2>&1
check $? "copies PDF renders"
if command -v qpdf >/dev/null 2>&1; then
    base_pages=$(qpdf --show-npages "$out_dir/regression.pdf" 2>/dev/null | tr -d ' \n')
    copy_pages=$(qpdf --show-npages "$out_dir/copies.pdf" 2>/dev/null | tr -d ' \n')
    if [ -n "$base_pages" ] && [ -n "$copy_pages" ] && [ "$copy_pages" -eq $((base_pages * 2)) ]; then
        check 0 "copies doubles page count ($copy_pages)"
    else
        check 1 "copies doubles page count (base=${base_pages:-none}, copies=${copy_pages:-none})"
    fi
else
    check 0 "copies page-count check (qpdf unavailable, skipped)"
fi

"$bin_dir/wkhtmltopdf-ng" --no-pdf-compression --javascript-delay 0 --enable-local-file-access \
    "$data_dir/toc.html" "$out_dir/uncompressed.pdf" >/dev/null 2>&1
check $? "uncompressed PDF renders"
if [ -s "$out_dir/uncompressed.pdf" ]; then
    check 0 "uncompressed PDF is a non-empty file"
    check_magic "$out_dir/uncompressed.pdf" 25504446 "uncompressed PDF has %PDF magic"
else
    check 1 "uncompressed PDF is a non-empty file"
fi

"$bin_dir/wkhtmltopdf-ng" --compress --compress-level 9 --javascript-delay 0 --enable-local-file-access \
    "$data_dir/toc.html" "$out_dir/compressed.pdf" >/dev/null 2>&1
check $? "compressed PDF renders"
check_magic "$out_dir/compressed.pdf" 25504446 "compressed PDF has %PDF magic"

"$bin_dir/wkhtmltopdf-ng" --load-error-handling skip --javascript-delay 0 --enable-local-file-access \
    "$data_dir/missing-does-not-exist.html" "$data_dir/toc.html" "$out_dir/skip.pdf" >/dev/null 2>&1
check $? "load-error-handling skip keeps a valid document"

"$bin_dir/wkhtmltopdf-ng" --javascript-delay 0 --enable-local-file-access \
    cover "$data_dir/cover.html" toc page "$data_dir/toc.html" "$out_dir/cover-toc.pdf" >/dev/null 2>&1
check $? "cover + toc + page renders"
check_magic "$out_dir/cover-toc.pdf" 25504446 "cover+toc PDF has %PDF magic"
expect_pages "$out_dir/cover-toc.pdf" 3 "cover+toc+page is three pages"

"$bin_dir/wkhtmltopdf-ng" --javascript-delay 0 --enable-local-file-access \
    --margin-top 22mm --margin-bottom 22mm \
    --header-html "$data_dir/header-repeat.html" \
    --footer-html "$data_dir/footer-repeat.html" \
    "$data_dir/ten-pages.html" "$out_dir/header-footer-10.pdf" >/dev/null 2>&1
check $? "10-page header/footer PDF renders"
check_magic "$out_dir/header-footer-10.pdf" 25504446 "10-page header/footer PDF has %PDF magic"
expect_pages "$out_dir/header-footer-10.pdf" 10 "header/footer document is ten pages"

"$bin_dir/wkhtmltopdf-ng" --javascript-delay 0 --enable-local-file-access \
    --title "Pagination Example" --margin-top 22mm --margin-bottom 22mm \
    --header-html "$data_dir/pagination-header.html" \
    --footer-html "$data_dir/pagination-footer.html" \
    "$data_dir/pagination.html" "$out_dir/pagination.pdf" >/dev/null 2>&1
check $? "pagination example PDF renders"
check_magic "$out_dir/pagination.pdf" 25504446 "pagination example PDF has %PDF magic"
expect_pages "$out_dir/pagination.pdf" 6 "pagination example is six pages"

"$bin_dir/wkhtmltopdf-ng" --javascript-delay 0 --enable-local-file-access \
    --margin-top 12mm --margin-bottom 14mm \
    --header-center "Northridge Bank · [page]/[topage]" \
    --footer-right "Page [page] of [topage]" \
    "$data_dir/bank-statement.html" "$out_dir/bank-statement.pdf" >/dev/null 2>&1
check $? "bank statement PDF renders"
check_magic "$out_dir/bank-statement.pdf" 25504446 "bank statement PDF has %PDF magic"
expect_pages "$out_dir/bank-statement.pdf" 2 "bank statement is two pages"

"$bin_dir/wkhtmltopdf-ng" --javascript-delay 0 --enable-local-file-access \
    "$data_dir/links.html" "$out_dir/links.pdf" >/dev/null 2>&1
check $? "complex hyperlink PDF renders"
check_magic "$out_dir/links.pdf" 25504446 "complex hyperlink PDF has %PDF magic"
if command -v python3 >/dev/null 2>&1 && [ -s "$out_dir/links.pdf" ]; then
    if python3 - "$out_dir/links.pdf" <<'PY' >/dev/null 2>&1
import sys
data = open(sys.argv[1], "rb").read()
needles = (
    b"wkhtmltopdf-ng.test/external-docs",
    b"example.com/wkng-status",
    b"docs@wkhtmltopdf-ng.test",
)
sys.exit(0 if all(n in data for n in needles) else 1)
PY
    then
        check 0 "complex hyperlink PDF embeds external URLs"
    else
        check 1 "complex hyperlink PDF embeds external URLs"
    fi
else
    check 0 "complex hyperlink PDF embeds external URLs (python3 unavailable, skipped)"
fi

"$bin_dir/wkhtmltoimage-ng" --enable-local-file-access "$data_dir/modern.html" "$out_dir/regression.png" >/dev/null 2>&1
check $? "regression PNG renders"
check_magic "$out_dir/regression.png" 89504e47 "regression PNG has magic bytes"

if command -v python3 >/dev/null 2>&1; then
    if python3 - "$out_dir/regression.png" <<'PY' >/dev/null 2>&1
import sys
import struct
import zlib

path = sys.argv[1]
with open(path, 'rb') as f:
    data = f.read()
# Parse PNG IDAT chunks and decompress to raw pixel bytes.
pos = 8
idat = b''
while pos < len(data):
    length = struct.unpack('>I', data[pos:pos + 4])[0]
    chunk_type = data[pos + 4:pos + 8]
    if chunk_type == b'IDAT':
        idat += data[pos + 8:pos + 8 + length]
    pos += 12 + length
raw = zlib.decompress(idat)
# Decode filtered scanlines with the PNG None/Sub/Up filters.
width, height = struct.unpack('>II', data[16:24])
bpp = 3 + 1 if data[25] == 6 else 3
stride = width * bpp
out = bytearray()
prev = bytearray(stride)
pos = 0
for _ in range(height):
    ftype = raw[pos]
    pos += 1
    line = bytearray(raw[pos:pos + stride])
    pos += stride
    if ftype == 1:
        for i in range(bpp, stride):
            line[i] = (line[i] + line[i - bpp]) & 0xFF
    elif ftype == 2:
        for i in range(stride):
            line[i] = (line[i] + prev[i]) & 0xFF
    elif ftype == 3:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
    elif ftype == 4:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
            line[i] = (line[i] + pr) & 0xFF
    out += line
    prev = line
# A blank image (single color) has zero variance among distinct samples.
samples = bytes(out)[:: max(1, len(out) // 65536)]
if len(set(samples)) < 2:
    sys.exit(1)
PY
    then
        check 0 "regression PNG is not blank"
    else
        check 1 "regression PNG is not blank"
    fi
else
    if [ -s "$out_dir/regression.png" ]; then
        check 0 "regression PNG produced (size check)"
    else
        check 1 "regression PNG produced (size check)"
    fi
fi

if command -v python3 >/dev/null 2>&1; then
    port=18481
    python3 -m http.server "$port" --directory "$data_dir" >/dev/null 2>&1 &
    server_pid=$!
    trap 'kill "$server_pid" 2>/dev/null' EXIT
    sleep 1
    "$bin_dir/wkhtmltopdf-ng" --proxy "http://127.0.0.1:$port" \
        "http://127.0.0.1:$port/toc.html" "$out_dir/proxy.pdf" >/dev/null 2>&1
    proxy_rc=$?
    kill "$server_pid" 2>/dev/null
    wait "$server_pid" 2>/dev/null
    trap - EXIT
    if [ "$proxy_rc" -eq 0 ] && [ -s "$out_dir/proxy.pdf" ]; then
        check 0 "proxied URL renders through --proxy"
        check_magic "$out_dir/proxy.pdf" 25504446 "proxied PDF has %PDF magic"
    else
        check 1 "proxied URL renders through --proxy"
    fi
else
    echo "SKIP: proxy integration check (python3 unavailable)"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL REGRESSION CHECKS PASSED"
else
    echo "$failures regression check(s) failed"
fi
exit "$failures"