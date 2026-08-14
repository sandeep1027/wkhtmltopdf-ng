#!/bin/bash
set -u

# Split / merge / insert-after-page.
# Usage: pdf_edit.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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
    if [ "$1" -eq 0 ]; then echo "PASS: $2"; else echo "FAIL: $2"; failures=$((failures + 1)); fi
}

text_of() {
    gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$1" 2>/dev/null | tr -s ' \n' ' '
}

orig="$out_dir/original-4.pdf"
ins="$out_dir/insert-2.pdf"
if ! "$bin" --enable-local-file-access --javascript-delay 0 \
        "$data_dir/pdf-edit-original.html" "$orig" >/dev/null 2>&1 || [ ! -s "$orig" ]; then
    check 1 "render 4-page original"
    echo "$failures pdf edit check(s) failed"
    exit "$failures"
fi
if ! "$bin" --enable-local-file-access --javascript-delay 0 \
        "$data_dir/pdf-edit-insert.html" "$ins" >/dev/null 2>&1 || [ ! -s "$ins" ]; then
    check 1 "render 2-page insert"
    echo "$failures pdf edit check(s) failed"
    exit "$failures"
fi
check 0 "render source PDFs"
[ "$(qpdf --show-npages "$orig" 2>/dev/null | tr -d ' \n')" = "4" ]
check $? "original is 4 pages"
[ "$(qpdf --show-npages "$ins" 2>/dev/null | tr -d ' \n')" = "2" ]
check $? "insert is 2 pages"

# Insert extra after page 3: 1,2,3, INSERT-A, INSERT-B, 4
spliced="$out_dir/insert-after-3.pdf"
if ! "$bin" --insert-pdf "$ins" --after-page 3 "$orig" "$spliced" >/dev/null 2>&1 || [ ! -s "$spliced" ]; then
    check 1 "insert after page 3"
else
    pages=$(qpdf --show-npages "$spliced" 2>/dev/null | tr -d ' \n')
    [ "$pages" = "6" ]
    check $? "spliced PDF is 6 pages (got ${pages:-none})"
    compact=$(text_of "$spliced")
    echo "$compact" | grep -q 'ORIG-PAGE-1' && echo "$compact" | grep -q 'ORIG-PAGE-3' \
        && echo "$compact" | grep -q 'INSERT-A' && echo "$compact" | grep -q 'INSERT-B' \
        && echo "$compact" | grep -q 'ORIG-PAGE-4'
    check $? "spliced PDF has original and insert markers"
    python3 - "$compact" <<'PY'
import sys
t = sys.argv[1]
order = []
for mark in ("ORIG-PAGE-1", "ORIG-PAGE-2", "ORIG-PAGE-3", "INSERT-A", "INSERT-B", "ORIG-PAGE-4"):
    i = t.find(mark)
    if i < 0:
        sys.exit(1)
    order.append(i)
sys.exit(0 if order == sorted(order) else 1)
PY
    check $? "page order is 1,2,3,insert,insert,4"
fi

# --before-page 4 is the same splice
before="$out_dir/insert-before-4.pdf"
if ! "$bin" --insert-pdf "$ins" --before-page 4 "$orig" "$before" >/dev/null 2>&1; then
    check 1 "insert before page 4"
else
    [ "$(qpdf --show-npages "$before" 2>/dev/null | tr -d ' \n')" = "6" ]
    check $? "before-page 4 is also 6 pages"
fi

# Merge
merged="$out_dir/merged.pdf"
if ! "$bin" --merge-pdf "$orig" "$ins" "$merged" >/dev/null 2>&1 || [ ! -s "$merged" ]; then
    check 1 "merge two PDFs"
else
    [ "$(qpdf --show-npages "$merged" 2>/dev/null | tr -d ' \n')" = "6" ]
    check $? "merged PDF is 6 pages"
    compact=$(text_of "$merged")
    echo "$compact" | grep -q 'ORIG-PAGE-4' && echo "$compact" | grep -q 'INSERT-A'
    check $? "merged PDF contains both documents"
fi

# Split range
part="$out_dir/pages-1-2.pdf"
if ! "$bin" --split-pdf --page-ranges 1-2 "$orig" "$part" >/dev/null 2>&1 || [ ! -s "$part" ]; then
    check 1 "split page-ranges 1-2"
else
    [ "$(qpdf --show-npages "$part" 2>/dev/null | tr -d ' \n')" = "2" ]
    check $? "range split is 2 pages"
    compact=$(text_of "$part")
    echo "$compact" | grep -q 'ORIG-PAGE-1' && echo "$compact" | grep -q 'ORIG-PAGE-2' \
        && ! echo "$compact" | grep -q 'ORIG-PAGE-4'
    check $? "range split has pages 1-2 only"
fi

# Split each page (clean dir so leftover names do not count)
split_dir="$out_dir/leaves"
rm -rf "$split_dir"
mkdir -p "$split_dir"
if ! "$bin" --split-pdf --split-pages "$orig" "$split_dir/leaf.pdf" >/dev/null 2>&1; then
    check 1 "split into single-page files"
else
    leaves=$(find "$split_dir" -name 'leaf*.pdf' | wc -l | tr -d ' ')
    [ "$leaves" = "4" ]
    check $? "split-pages wrote 4 files (got ${leaves:-none})"
    first=$(ls "$split_dir"/leaf*.pdf 2>/dev/null | sort | head -1)
    [ -n "$first" ] && [ "$(qpdf --show-npages "$first" 2>/dev/null | tr -d ' \n')" = "1" ]
    check $? "first leaf is 1 page"
fi

if ! "$bin" --insert-pdf "$ins" "$orig" "$out_dir/bad.pdf" >/dev/null 2>&1; then
    check 0 "insert without --after-page fails"
else
    check 1 "insert without --after-page fails"
fi

if [ "$failures" -eq 0 ]; then echo "ALL PDF EDIT CHECKS PASSED"; else echo "$failures pdf edit check(s) failed"; fi
exit "$failures"
