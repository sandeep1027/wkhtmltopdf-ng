#!/bin/bash
set -u

# TOC page numbers, --dump-info JSON, and --attach PDF attachments.
# Usage: extra_features.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

if ! command -v qpdf >/dev/null 2>&1; then
    check 1 "qpdf is required for extra feature checks"
    echo "$failures extra feature check(s) failed"
    exit "$failures"
fi
if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required for extra feature checks"
    echo "$failures extra feature check(s) failed"
    exit "$failures"
fi

page1_text() {
    gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite \
        -dFirstPage=1 -dLastPage=1 \
        -sOutputFile=- "$1" 2>/dev/null | python3 -c '
import re, sys
text = sys.stdin.read().replace("\ufb01", "fi").replace("\ufb02", "fl")
print(re.sub(r"\s+", "", text))
'
}

# --- TOC page numbers -----------------------------------------------------
toc_pdf="$out_dir/extra-toc.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        toc "$data_dir/toc.html" "$toc_pdf" >/dev/null 2>&1 \
        || [ ! -s "$toc_pdf" ]; then
    check 1 "TOC with page numbers renders"
else
    check 0 "TOC with page numbers renders"
fi

if [ -s "$toc_pdf" ]; then
    pages=$(qpdf --show-npages "$toc_pdf" 2>/dev/null | tr -d ' \n')
    if [ "$pages" = "2" ]; then
        check 0 "TOC PDF has 2 pages"
    else
        check 1 "TOC PDF has 2 pages (got ${pages:-none})"
    fi
    text=$(page1_text "$toc_pdf")
    python3 - "$text" <<'PY'
import re
import sys

text = sys.argv[1]
if "Introduction" in text and re.search(r"\d", text):
    print("TOC_PAGE_NUMBERS:PASS")
    sys.exit(0)
print("TOC_PAGE_NUMBERS:FAIL (expected heading with a page number)")
sys.exit(1)
PY
    check $? "TOC lists headings with page numbers"
fi

# --- --no-toc-page-numbers -------------------------------------------------
toc_nonum="$out_dir/extra-toc-nonum.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access --no-toc-page-numbers \
        toc "$data_dir/toc.html" "$toc_nonum" >/dev/null 2>&1 \
        || [ ! -s "$toc_nonum" ]; then
    check 1 "TOC without page numbers renders"
else
    check 0 "TOC without page numbers renders"
fi

if [ -s "$toc_nonum" ]; then
    text=$(page1_text "$toc_nonum")
    python3 - "$text" <<'PY'
import re
import sys

text = sys.argv[1]
if "Introduction" in text and not re.search(r"\d", text):
    print("TOC_NO_PAGE_NUMBERS:PASS")
    sys.exit(0)
print("TOC_NO_PAGE_NUMBERS:FAIL (expected no digits on the TOC page)")
sys.exit(1)
PY
    check $? "--no-toc-page-numbers removes the numbers"
fi

# --- --dump-info JSON ------------------------------------------------------
info_json="$out_dir/extra-info.json"
if ! "$bin" --dump-info "$info_json" --javascript-delay 0 --enable-local-file-access \
        --title "Extra Features" "$data_dir/toc.html" "$out_dir/extra-info.pdf" \
        >/dev/null 2>&1 || [ ! -s "$info_json" ]; then
    check 1 "--dump-info writes JSON"
else
    check 0 "--dump-info writes JSON"
fi

if [ -s "$info_json" ]; then
    python3 - "$info_json" <<'PY'
import json
import sys

with open(sys.argv[1], "r", encoding="utf-8") as handle:
    info = json.load(handle)
if info.get("pages") == 1 and info.get("title") == "Extra Features":
    print("INFO_JSON_PAGES:PASS")
else:
    print("INFO_JSON_PAGES:FAIL (expected pages 1, title Extra Features)")
    sys.exit(1)
outline = info.get("outline") or []
if outline and outline[0].get("title") == "Introduction" and outline[0].get("page") >= 1:
    print("INFO_JSON_OUTLINE:PASS")
else:
    print("INFO_JSON_OUTLINE:FAIL (expected outline with Introduction)")
    sys.exit(1)
sys.exit(0)
PY
    check $? "dump-info JSON has page count, title, and outline"
fi

# --- --attach --------------------------------------------------------------
echo "extra feature attachment" > "$out_dir/attach-note.txt"
attach_pdf="$out_dir/extra-attached.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --attach "$out_dir/attach-note.txt" \
        "$data_dir/toc.html" "$attach_pdf" >/dev/null 2>&1 \
        || [ ! -s "$attach_pdf" ]; then
    check 1 "--attach embeds a file"
else
    check 0 "--attach embeds a file"
fi

if [ -s "$attach_pdf" ]; then
    python3 - "$attach_pdf" <<'PY'
import sys

data = open(sys.argv[1], "rb").read()
if b"/EmbeddedFiles" in data and b"attach-note.txt" in data:
    print("ATTACH_EMBEDDED:PASS")
    sys.exit(0)
print("ATTACH_EMBEDDED:FAIL (expected /EmbeddedFiles and the attachment name)")
sys.exit(1)
PY
    check $? "attachment present in the output PDF"
fi

# --- --attach combined with encryption -------------------------------------
enc_pdf="$out_dir/extra-attached-enc.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --user-password secret --attach "$out_dir/attach-note.txt" \
        "$data_dir/toc.html" "$enc_pdf" >/dev/null 2>&1 \
        || [ ! -s "$enc_pdf" ]; then
    check 1 "--attach with --user-password renders"
else
    check 0 "--attach with --user-password renders"
fi

if [ -s "$enc_pdf" ]; then
    python3 - "$enc_pdf" <<'PY'
import subprocess
import sys

pdf = sys.argv[1]
data = open(pdf, "rb").read()
if b"/EmbeddedFiles" not in data:
    print("ATTACH_ENC:FAIL (expected /EmbeddedFiles)")
    sys.exit(1)
# The file must decrypt with the user password.
result = subprocess.run(["qpdf", "--password=secret", pdf, "/dev/null"],
                        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
if result.returncode == 0:
    print("ATTACH_ENC:PASS")
    sys.exit(0)
print("ATTACH_ENC:FAIL (encrypted attachment PDF does not decrypt)")
sys.exit(1)
PY
    check $? "attachment + encryption combine correctly"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL EXTRA FEATURE CHECKS PASSED"
else
    echo "$failures extra feature check(s) failed"
fi
exit "$failures"
