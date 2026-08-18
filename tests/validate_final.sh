#!/bin/bash
set -u

# Dedicated validation: repeating thead, fixed header/footer, local WOFF2.
# Usage: validate_final.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

page_text() {
    gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite \
        -dFirstPage="$2" -dLastPage="$2" -sOutputFile=- "$1" 2>/dev/null | tr -s ' \n' ' '
}

# --- 1. thead ---
thead_pdf="$out_dir/validate-thead.pdf"
if ! "$bin" --javascript-delay 0 --window-status thead-ready --compress \
        "$data_dir/validate-thead.html" "$thead_pdf" >/dev/null 2>&1 || [ ! -s "$thead_pdf" ]; then
    check 1 "thead PDF renders"
else
    check 0 "thead PDF renders"
    pages=$(qpdf --show-npages "$thead_pdf" 2>/dev/null | tr -d ' \n')
    if [ "${pages:-0}" -ge 2 ]; then
        check 0 "thead table spans 2+ pages ($pages)"
    else
        check 1 "thead table spans 2+ pages (got ${pages:-none})"
    fi
    p1=$(page_text "$thead_pdf" 1)
    p2=$(page_text "$thead_pdf" 2)
    case "$p1" in *COL-A*COL-B*COL-C*COL-D*ROW-1*) check 0 "page 1 has thead + first rows" ;;
        *) check 1 "page 1 has thead + first rows" ;; esac
    case "$p2" in *COL-A*COL-B*COL-C*COL-D*) check 0 "page 2 repeats thead" ;;
        *) check 1 "page 2 repeats thead" ;; esac
    case "$p2" in *ROW-1*) check 1 "page 2 is not a reprint of row 1" ;;
        *) check 0 "page 2 continues after page 1" ;; esac
fi

# --- 2. fixed header/footer (no negative margins) ---
fixed_pdf="$out_dir/validate-fixed.pdf"
if ! "$bin" --javascript-delay 0 --compress \
        "$data_dir/validate-fixed.html" "$fixed_pdf" >/dev/null 2>&1 || [ ! -s "$fixed_pdf" ]; then
    check 1 "fixed chrome PDF renders"
else
    check 0 "fixed chrome PDF renders"
    pages=$(qpdf --show-npages "$fixed_pdf" 2>/dev/null | tr -d ' \n')
    if [ "${pages:-0}" -eq 3 ]; then
        check 0 "fixed chrome is 3 pages"
    else
        check 1 "fixed chrome is 3 pages (got ${pages:-none})"
    fi
    for p in 1 2 3; do
        t=$(page_text "$fixed_pdf" "$p")
        case "$t" in *FIXED-HEADER*) check 0 "page $p has FIXED-HEADER" ;;
            *) check 1 "page $p has FIXED-HEADER" ;; esac
        case "$t" in *FIXED-FOOTER*) check 0 "page $p has FIXED-FOOTER" ;;
            *) check 1 "page $p has FIXED-FOOTER" ;; esac
        case "$t" in *P${p}-START*) check 0 "page $p body start is present" ;;
            *) check 1 "page $p body start is present" ;; esac
    done
fi

# --- 3. local WOFF2 ---
woff_pdf="$out_dir/validate-woff2.pdf"
if ! "$bin" --enable-local-file-access --window-status woff2-ready --compress \
        "$data_dir/validate-woff2.html" "$woff_pdf" >/dev/null 2>&1 || [ ! -s "$woff_pdf" ]; then
    check 1 "woff2 PDF renders"
else
    check 0 "woff2 PDF renders"
    t=$(page_text "$woff_pdf" 1 | tr -d ' ')
    case "$t" in *FONT-WOFF2-READY*) check 0 "document.fonts.ready + check(PDFTestFont)" ;;
        *) check 1 "document.fonts.ready + check(PDFTestFont) (got $t)" ;; esac
    case "$t" in *CUSTOMWOFF2*) check 0 "custom WOFF2 sample text present" ;;
        *) check 1 "custom WOFF2 sample text present" ;; esac
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL FINAL VALIDATION CHECKS PASSED"
else
    echo "$failures final validation check(s) failed"
fi
exit "$failures"
