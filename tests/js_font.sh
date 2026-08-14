#!/bin/bash
set -u

# JS FontFace + @font-face fixture, then a 60-page JS-generated document.
# Usage: js_font.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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
        -dFirstPage="${2:-1}" -dLastPage="${3:-99}" \
        -sOutputFile=- "$1" 2>/dev/null
}

pdf="$out_dir/js-font.pdf"
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 200 --window-status js-font-done \
        "$data_dir/js-font.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "JS font PDF renders"
else
    check 0 "JS font PDF renders"
    text=$(page_text "$pdf")
    case "$text" in *JS-FONT-READY*) check 0 "JS set JS-FONT-READY after FontFace.load" ;;
        *) check 1 "JS set JS-FONT-READY after FontFace.load" ;; esac
    case "$text" in *JS-FONT-FAILED*) check 1 "JS font load did not fail" ;;
        *) check 0 "JS font load did not fail" ;; esac
    case "$text" in *JS\ FontFace\ loaded*) check 0 "JS replaced heading with loaded family" ;;
        *) check 1 "JS replaced heading with loaded family" ;; esac
fi

pdf60="$out_dir/js-font-60.pdf"
start=$(date +%s.%N)
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 200 --window-status js-font-60-done \
        "$data_dir/js-font-60.html" "$pdf60" >/dev/null 2>&1 || [ ! -s "$pdf60" ]; then
    check 1 "JS font 60-page PDF renders"
else
    elapsed=$(python3 -c "print('%.2f' % ($(date +%s.%N) - $start))")
    check 0 "JS font 60-page PDF renders in ${elapsed}s"
    if command -v qpdf >/dev/null 2>&1; then
        pages=$(qpdf --show-npages "$pdf60" 2>/dev/null | tr -d ' \n')
        if [ "$pages" = "60" ]; then
            check 0 "JS built 60 pages ($elapsed s)"
        else
            check 1 "JS built 60 pages (got ${pages:-none})"
        fi
    fi
    text=$(page_text "$pdf60" 1 1)
    case "$text" in *JS-FONT-60-READY*) check 0 "page 1 has JS-FONT-60-READY" ;;
        *) check 1 "page 1 has JS-FONT-60-READY" ;; esac
    text60=$(page_text "$pdf60" 60 60)
    case "$text60" in *JS-PAGE-60*) check 0 "page 60 has JS-PAGE-60" ;;
        *) check 1 "page 60 has JS-PAGE-60" ;; esac
    echo "TIME: js-font-60 ${elapsed}s pages=$(qpdf --show-npages "$pdf60" 2>/dev/null | tr -d ' \n') size=$(wc -c < "$pdf60")"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL JS FONT CHECKS PASSED"
else
    echo "$failures JS font check(s) failed"
fi
exit "$failures"
