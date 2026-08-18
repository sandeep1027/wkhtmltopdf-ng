#!/bin/bash
set -u

# Each document object keeps its own header/footer text, and [page]/[topage]
# tokens resolve against the merged document. Cover pages draw no header.
# Usage: per_object_pages.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

pdf="$out_dir/per-object-page-numbers.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        "$data_dir/toc.html" --header-center "FIRST [page]/[topage]" \
        "$data_dir/multi-second.html" --header-center "SECOND [page]/[topage]" \
        "$pdf" >/dev/null 2>&1 \
        || [ ! -s "$pdf" ]; then
    check 1 "per-object page-number PDF renders"
    echo "1 per-object page-number check(s) failed"
    exit 1
fi
check 0 "per-object page-number PDF renders"

if ! command -v qpdf >/dev/null 2>&1; then
    check 1 "qpdf is required to inspect per-object page numbers"
    echo "$failures per-object page-number check(s) failed"
    exit "$failures"
fi

pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
if [ "$pages" = "2" ]; then
    check 0 "per-object PDF has 2 pages"
else
    check 1 "per-object PDF has 2 pages (got ${pages:-none})"
fi

if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required to read per-object header text"
    echo "$failures per-object page-number check(s) failed"
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

text1 = page_text(1)
if "FIRST1/2" in text1 and "SECOND" not in text1:
    print("PAGE_1:PASS")
else:
    print("PAGE_1:FAIL (expected FIRST 1/2, no SECOND)")
    failures += 1

text2 = page_text(2)
if "SECOND2/2" in text2 and "FIRST" not in text2:
    print("PAGE_2:PASS")
else:
    print("PAGE_2:FAIL (expected SECOND 2/2, no FIRST)")
    failures += 1

sys.exit(1 if failures else 0)
PY
check $? "each page carries its own object's header with merged page numbers"

cover_pdf="$out_dir/per-object-cover.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        cover "$data_dir/cover.html" \
        page "$data_dir/toc.html" --header-center "BODY [page]/[topage]" \
        "$cover_pdf" >/dev/null 2>&1 \
        || [ ! -s "$cover_pdf" ]; then
    check 1 "cover + page-number header PDF renders"
else
    check 0 "cover + page-number header PDF renders"
fi

if [ -s "$cover_pdf" ]; then
    python3 - "$cover_pdf" <<'PY'
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
    return re.sub(r"\s+", "", text)

if "BODY" in page_text(1):
    print("COVER_PAGE_1:FAIL (cover must not carry body header)")
    failures += 1
else:
    print("COVER_PAGE_1:PASS")

text2 = page_text(2)
if "BODY2/2" in text2:
    print("COVER_PAGE_2:PASS")
else:
    print("COVER_PAGE_2:FAIL (expected BODY 2/2)")
    failures += 1

sys.exit(1 if failures else 0)
PY
    check $? "cover page draws no header; body pages keep theirs"
fi

# --page-ranges keeps merged-document page numbers on the surviving pages.
ranged_pdf="$out_dir/per-object-ranged.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access --page-ranges 2 \
        "$data_dir/toc.html" --header-center "FIRST [page]/[topage]" \
        "$data_dir/multi-second.html" --header-center "SECOND [page]/[topage]" \
        "$ranged_pdf" >/dev/null 2>&1 \
        || [ ! -s "$ranged_pdf" ]; then
    check 1 "page-ranges + per-object headers PDF renders"
else
    check 0 "page-ranges + per-object headers PDF renders"
fi

if [ -s "$ranged_pdf" ]; then
    ranged_pages=$(qpdf --show-npages "$ranged_pdf" 2>/dev/null | tr -d ' \n')
    if [ "$ranged_pages" = "1" ]; then
        check 0 "page-ranges PDF has 1 page"
    else
        check 1 "page-ranges PDF has 1 page (got ${ranged_pages:-none})"
    fi
    python3 - "$ranged_pdf" <<'PY'
import re
import subprocess
import sys

pdf = sys.argv[1]
raw = subprocess.check_output(
    ["gs", "-q", "-dNOPAUSE", "-dBATCH", "-sDEVICE=txtwrite",
     "-sOutputFile=-", pdf],
    stderr=subprocess.DEVNULL)
text = re.sub(r"\s+", "", raw.decode("utf-8", "replace"))
if "SECOND2/2" in text and "FIRST" not in text:
    print("RANGED:PASS")
    sys.exit(0)
print("RANGED:FAIL (expected SECOND 2/2 from the full document)")
sys.exit(1)
PY
    check $? "surviving page keeps its own header with full-document numbering"
fi

# The watermark lands on every page, including the cover; the cover still
# carries no header/footer.
wm_pdf="$out_dir/per-object-watermark.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access --watermark "DRAFT" \
        cover "$data_dir/cover.html" \
        page "$data_dir/toc.html" --header-center "BODY [page]/[topage]" \
        "$wm_pdf" >/dev/null 2>&1 \
        || [ ! -s "$wm_pdf" ]; then
    check 1 "cover + watermark PDF renders"
else
    check 0 "cover + watermark PDF renders"
fi

if [ -s "$wm_pdf" ]; then
    python3 - "$wm_pdf" <<'PY'
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
    return re.sub(r"\s+", "", raw.decode("utf-8", "replace"))

def has_watermark(text):
    # The diagonal watermark is rotated, so text extractors may reverse it.
    return "DRAFT" in text or "TFARD" in text

text1 = page_text(1)
if has_watermark(text1) and "BODY" not in text1:
    print("WM_PAGE_1:PASS")
else:
    print("WM_PAGE_1:FAIL (expected watermark, no body header on cover)")
    failures += 1

text2 = page_text(2)
if has_watermark(text2) and "BODY2/2" in text2:
    print("WM_PAGE_2:PASS")
else:
    print("WM_PAGE_2:FAIL (expected watermark and BODY 2/2)")
    failures += 1

sys.exit(1 if failures else 0)
PY
    check $? "watermark covers all pages; cover still has no header"
fi

# Mixed objects: one uses page-number placeholders (qpdf overlay), the other
# a plain CSS header. The plain header must not be drawn twice.
mixed_pdf="$out_dir/per-object-mixed.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        "$data_dir/toc.html" --header-center "NUMBERED [page]/[topage]" \
        "$data_dir/multi-second.html" --header-center "PLAIN" \
        "$mixed_pdf" >/dev/null 2>&1 \
        || [ ! -s "$mixed_pdf" ]; then
    check 1 "mixed numbered/plain headers PDF renders"
else
    check 0 "mixed numbered/plain headers PDF renders"
fi

if [ -s "$mixed_pdf" ]; then
    python3 - "$mixed_pdf" <<'PY'
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
    return re.sub(r"\s+", "", raw.decode("utf-8", "replace"))

text1 = page_text(1)
if "NUMBERED1/2" in text1 and "PLAIN" not in text1:
    print("MIXED_PAGE_1:PASS")
else:
    print("MIXED_PAGE_1:FAIL (expected NUMBERED 1/2, no PLAIN)")
    failures += 1

text2 = page_text(2)
if text2.count("PLAIN") == 1 and "NUMBERED" not in text2:
    print("MIXED_PAGE_2:PASS")
else:
    print("MIXED_PAGE_2:FAIL (PLAIN must appear exactly once)")
    failures += 1

sys.exit(1 if failures else 0)
PY
    check $? "placeholder object uses the overlay; plain header is not double-drawn"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL PER-OBJECT PAGE-NUMBER CHECKS PASSED"
else
    echo "$failures per-object page-number check(s) failed"
fi
exit "$failures"
