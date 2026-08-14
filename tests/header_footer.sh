#!/bin/bash
set -u

# Convert a 10-page document with separate header-html and footer-html files
# and check that both bands repeat on every page with the correct page number.
# Usage: header_footer.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

pdf="$out_dir/header-footer-10.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --margin-top 22mm --margin-bottom 22mm \
        --header-html "$data_dir/header-repeat.html" \
        --footer-html "$data_dir/footer-repeat.html" \
        "$data_dir/ten-pages.html" "$pdf" >/dev/null 2>&1 \
        || [ ! -s "$pdf" ]; then
    check 1 "10-page header/footer PDF renders"
    echo "1 header/footer check(s) failed"
    exit 1
fi
check 0 "10-page header/footer PDF renders"

if ! command -v qpdf >/dev/null 2>&1; then
    check 1 "qpdf is required to inspect repeating header/footer pages"
    echo "$failures header/footer check(s) failed"
    exit "$failures"
fi

pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
if [ "$pages" = "10" ]; then
    check 0 "PDF has 10 pages"
else
    check 1 "PDF has 10 pages (got ${pages:-none})"
fi

if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required to read repeating header/footer text"
    echo "$failures header/footer check(s) failed"
    exit "$failures"
fi

python3 - "$pdf" <<'PY'
import re
import subprocess
import sys

pdf = sys.argv[1]
failures = 0

def page_text(page):
    raw = subprocess.check_output(
        ["gs", "-q", "-dNOPAUSE", "-dBATCH", "-sDEVICE=txtwrite",
         "-dFirstPage=%d" % page, "-dLastPage=%d" % page,
         "-sOutputFile=-", pdf],
        stderr=subprocess.DEVNULL)
    text = raw.decode("utf-8", "replace")
    text = text.replace("\ufb01", "fi").replace("\ufb02", "fl")
    return re.sub(r"\s+", "", text)

for page in range(1, 11):
    text = page_text(page)
    missing = []
    if "WKNG-HEADER-REPEAT" not in text:
        missing.append("header")
    if "WKNG-FOOTER-REPEAT" not in text:
        missing.append("footer")
    if ("CHAPTER-%02d" % page) not in text:
        missing.append("CHAPTER-%02d" % page)
    if ("Page%dof10" % page) not in text:
        missing.append("Page %d of 10" % page)
    if ("Sheet%d/10" % page) not in text:
        missing.append("Sheet %d/10" % page)
    if missing:
        print("PAGE_%d:FAIL %s" % (page, ",".join(missing)))
        failures += 1
    else:
        print("PAGE_%d:PASS" % page)
sys.exit(1 if failures else 0)
PY
check $? "header and footer HTML repeat on all 10 pages with matching numbers"

if [ "$failures" -eq 0 ]; then
    echo "ALL HEADER/FOOTER CHECKS PASSED"
else
    echo "$failures header/footer check(s) failed"
fi
exit "$failures"
