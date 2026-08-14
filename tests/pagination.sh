#!/bin/bash
set -u

# Pagination example: separate header/footer HTML, plus classic text tokens.
# Usage: pagination.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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
    local pdf=$1
    local page=$2
    gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite \
        -dFirstPage="$page" -dLastPage="$page" \
        -sOutputFile=- "$pdf" 2>/dev/null | python3 -c '
import re, sys
text = sys.stdin.read().replace("\ufb01", "fi").replace("\ufb02", "fl")
print(re.sub(r"\s+", "", text))
'
}

html_pdf="$out_dir/pagination.html.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --title "Pagination Example" \
        --margin-top 22mm --margin-bottom 22mm \
        --header-html "$data_dir/pagination-header.html" \
        --footer-html "$data_dir/pagination-footer.html" \
        "$data_dir/pagination.html" "$html_pdf" >/dev/null 2>&1 \
        || [ ! -s "$html_pdf" ]; then
    check 1 "HTML pagination example renders"
else
    check 0 "HTML pagination example renders"
fi

token_pdf="$out_dir/pagination.tokens.pdf"
if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --margin-top 18mm --margin-bottom 18mm \
        --header-left "PAGINATION-TEXT" \
        --header-right "[page]/[topage]" \
        --footer-center "Page [page] of [topage]" \
        "$data_dir/pagination.html" "$token_pdf" >/dev/null 2>&1 \
        || [ ! -s "$token_pdf" ]; then
    check 1 "text-token pagination example renders"
else
    check 0 "text-token pagination example renders"
fi

if ! command -v qpdf >/dev/null 2>&1; then
    check 1 "qpdf is required to inspect pagination"
    echo "$failures pagination check(s) failed"
    exit "$failures"
fi

if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required to read pagination text"
    echo "$failures pagination check(s) failed"
    exit "$failures"
fi

html_pages=$(qpdf --show-npages "$html_pdf" 2>/dev/null | tr -d ' \n')
token_pages=$(qpdf --show-npages "$token_pdf" 2>/dev/null | tr -d ' \n')
if [ "$html_pages" = "6" ]; then
    check 0 "HTML pagination PDF has 6 pages"
else
    check 1 "HTML pagination PDF has 6 pages (got ${html_pages:-none})"
fi
if [ "$token_pages" = "6" ]; then
    check 0 "text-token pagination PDF has 6 pages"
else
    check 1 "text-token pagination PDF has 6 pages (got ${token_pages:-none})"
fi

html_fail=0
token_fail=0
[ -s "$html_pdf" ] || html_fail=1
[ -s "$token_pdf" ] || token_fail=1
for page in 1 2 3 4 5 6; do
    if [ -s "$html_pdf" ]; then
        text=$(page_text "$html_pdf" "$page")
        missing=
        case "$text" in *PAGINATION-HEADER*) ;; *) missing="$missing header" ;; esac
        case "$text" in *PAGINATION-FOOTER*) ;; *) missing="$missing footer" ;; esac
        case "$text" in *PAGINATION-PAGE-0$page*) ;; *) missing="$missing body" ;; esac
        case "$text" in *${page}/6*) ;; *) missing="$missing $page/6" ;; esac
        case "$text" in *from1*) ;; *) missing="$missing frompage" ;; esac
        if [ -n "$missing" ]; then
            echo "FAIL: HTML page $page missing$missing"
            html_fail=$((html_fail + 1))
        else
            echo "PASS: HTML page $page pagination"
        fi
    fi
    if [ -s "$token_pdf" ]; then
        text=$(page_text "$token_pdf" "$page")
        missing=
        case "$text" in *PAGINATION-TEXT*) ;; *) missing="$missing header-left" ;; esac
        case "$text" in *Page${page}of6*) ;; *) missing="$missing footer Page $page of 6" ;; esac
        case "$text" in *${page}/6*) ;; *) missing="$missing $page/6" ;; esac
        if [ -n "$missing" ]; then
            echo "FAIL: token page $page missing$missing"
            token_fail=$((token_fail + 1))
        else
            echo "PASS: token page $page pagination"
        fi
    fi
done
check "$html_fail" "HTML header/footer page numbers advance on all 6 pages"
check "$token_fail" "text-token page numbers advance on all 6 pages"

if [ "$failures" -eq 0 ]; then
    echo "ALL PAGINATION CHECKS PASSED"
else
    echo "$failures pagination check(s) failed"
fi
exit "$failures"
