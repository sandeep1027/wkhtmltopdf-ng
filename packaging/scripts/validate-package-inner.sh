#!/bin/bash
set -u

# Clean-install validation. Runs inside a fresh container where the built
# .deb is mounted at /pkg. Installs the package with apt (resolving the
# declared Qt/qpdf dependencies) and renders real PDFs and images through
# the installed binaries, including the wkhtmltopdf/wkhtmltoimage
# compatibility symlinks and the qpdf-dependent overlay pipeline.

export DEBIAN_FRONTEND=noninteractive
export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox

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

check_pages() {
    local file=$1
    local expected=$2
    local label=$3
    local pages
    pages=$(qpdf --show-npages "$file" 2>/dev/null | tr -d ' \n')
    if [ "$pages" = "$expected" ]; then
        check 0 "$label"
    else
        check 1 "$label"
    fi
}

echo "== Installing package =="
apt-get update >/dev/null 2>&1
apt-get install -y /pkg/*.deb >/dev/null 2>&1
check $? "package and dependencies install"
apt-get install -y python3 >/dev/null 2>&1

echo "== Version smoke tests =="
check "$(wkhtmltopdf-ng --version >/dev/null 2>&1; echo $?)" "wkhtmltopdf-ng --version"
check "$(wkhtmltopdf --version >/dev/null 2>&1; echo $?)" "wkhtmltopdf --version (symlink)"
check "$(wkhtmltoimage-ng --version >/dev/null 2>&1; echo $?)" "wkhtmltoimage-ng --version"
check "$(wkhtmltoimage --version >/dev/null 2>&1; echo $?)" "wkhtmltoimage --version (symlink)"

echo "== Real renders =="
cat > /tmp/page.html <<'EOF'
<!doctype html>
<html>
<head><meta charset="utf-8"><title>Clean install test</title></head>
<body><h1>Clean install</h1><p>Rendered from a fresh container.</p>
<div style="background:#3f7d3f;height:260px;width:100%"></div></body>
</html>
EOF

wkhtmltopdf-ng --enable-local-file-access /tmp/page.html /tmp/out.pdf >/dev/null 2>&1
check $? "wkhtmltopdf-ng renders PDF"
check_magic /tmp/out.pdf 25504446 "PDF has %PDF magic"
check_pages /tmp/out.pdf 1 "qpdf reads single-page PDF"

wkhtmltoimage-ng --enable-local-file-access /tmp/page.html /tmp/out.png >/dev/null 2>&1
check $? "wkhtmltoimage-ng renders PNG"
check_magic /tmp/out.png 89504e47 "PNG has magic bytes"
if python3 - /tmp/out.png <<'PY' >/dev/null 2>&1
import sys, struct, zlib
data = open(sys.argv[1], 'rb').read()
w, h = struct.unpack('>II', data[16:24])
pos = 8
idat = b''
while pos < len(data):
    ln = struct.unpack('>I', data[pos:pos + 4])[0]
    t = data[pos + 4:pos + 8]
    if t == b'IDAT':
        idat += data[pos + 8:pos + 8 + ln]
    pos += 12 + ln
raw = zlib.decompress(idat)
bpp = 4 if data[25] == 6 else 3
stride = w * bpp
out = bytearray()
prev = bytearray(stride)
p = 0
for _ in range(h):
    ft = raw[p]
    p += 1
    line = bytearray(raw[p:p + stride])
    p += stride
    if ft == 1:
        for i in range(bpp, stride):
            line[i] = (line[i] + line[i - bpp]) & 0xFF
    elif ft == 2:
        for i in range(stride):
            line[i] = (line[i] + prev[i]) & 0xFF
    elif ft == 3:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
    elif ft == 4:
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            pr = a + b - c
            pa, pb, pc = abs(pr - a), abs(pr - b), abs(pr - c)
            line[i] = (line[i] + (a if (pa <= pb and pa <= pc) else (b if pb <= pc else c))) & 0xFF
    out += line
    prev = line
samples = bytes(out)[:: max(1, len(out) // 65536)]
if len(set(samples)) < 2:
    sys.exit(1)
PY
then
    check 0 "PNG is not blank"
else
    check 1 "PNG is not blank"
fi

wkhtmltopdf --enable-local-file-access /tmp/page.html /tmp/out2.pdf >/dev/null 2>&1
check $? "wkhtmltopdf symlink renders PDF"
check_magic /tmp/out2.pdf 25504446 "symlink PDF has %PDF magic"

wkhtmltoimage --enable-local-file-access /tmp/page.html /tmp/out2.png >/dev/null 2>&1
check $? "wkhtmltoimage symlink renders PNG"

echo "== qpdf-dependent features =="
wkhtmltopdf --enable-local-file-access --header-center "[page]/[topage]" /tmp/page.html /tmp/out3.pdf >/dev/null 2>&1
check $? "header overlay pipeline runs"
check_pages /tmp/out3.pdf 1 "overlay PDF still one page"

wkhtmltopdf --enable-local-file-access --toc --outline /tmp/page.html /tmp/out4.pdf >/dev/null 2>&1
check $? "TOC/outline pipeline runs"
check_magic /tmp/out4.pdf 25504446 "TOC PDF has %PDF magic"

if [ "$failures" -eq 0 ]; then
    echo "ALL PACKAGE VALIDATION CHECKS PASSED"
else
    echo "$failures package validation check(s) failed"
fi
exit "$failures"
