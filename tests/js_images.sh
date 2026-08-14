#!/bin/bash
set -u

# Multiple images + JS metrics/clones.
# Usage: js_images.sh BINARY TEST_DATA_DIR OUTPUT_DIR

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

bin=${1:?binary required}
data_dir=${2:?test data dir required}
out_dir=${3:?output dir required}
mkdir -p "$out_dir"
failures=0
pdf="$out_dir/js-images.pdf"

check() {
    if [ "$1" -eq 0 ]; then
        echo "PASS: $2"
    else
        echo "FAIL: $2"
        failures=$((failures + 1))
    fi
}

start=$(date +%s.%N)
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 200 --window-status js-images-done \
        --load-media-error-handling ignore \
        "$data_dir/js-images.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "JS multi-image PDF renders"
    echo "1 JS image check(s) failed"
    exit 1
fi
elapsed=$(python3 -c "print('%.2f' % ($(date +%s.%N) - $start))")
check 0 "JS multi-image PDF renders in ${elapsed}s"

pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
if [ "${pages:-0}" -ge 2 ]; then
    check 0 "PDF has clone page ($pages pages)"
else
    check 1 "PDF has clone page (got ${pages:-none})"
fi

text=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$pdf" 2>/dev/null)
compact=$(printf '%s' "$text" | tr -d ' \n')

case "$compact" in *JS-IMAGES-READY*) check 0 "JS finished (JS-IMAGES-READY)" ;;
    *) check 1 "JS finished (JS-IMAGES-READY)" ;; esac
case "$compact" in *JS-LOADED=7*) check 0 "JS counted 7 loaded images" ;;
    *) check 1 "JS counted 7 loaded images" ;; esac
case "$compact" in *JS-BROKEN=1*) check 0 "JS counted 1 broken image" ;;
    *) check 1 "JS counted 1 broken image" ;; esac
case "$compact" in *JS-CLONES=7*) check 0 "JS cloned 7 images onto page 2" ;;
    *) check 1 "JS cloned 7 images onto page 2" ;; esac
case "$compact" in *B1,G1,ICON,LOGO,R1,SEAL,Y1*) check 0 "JS sku list includes all good images" ;;
    *) check 1 "JS sku list includes all good images" ;; esac
case "$compact" in *JS-IMAGES-FAILED*) check 1 "JS did not fail" ;;
    *) check 0 "JS did not fail" ;; esac

if python3 - "$pdf" <<'PY' >/dev/null 2>&1
import sys
data = open(sys.argv[1], "rb").read()
# rasters and/or form xobjects from svg+png
sys.exit(0 if data.count(b"/XObject") >= 1 else 1)
PY
then
    check 0 "PDF embeds image/graphics XObjects"
else
    check 1 "PDF embeds image/graphics XObjects"
fi

echo "TIME: js-images ${elapsed}s pages=$pages size=$(wc -c < "$pdf")"

if [ "$failures" -eq 0 ]; then
    echo "ALL JS IMAGE CHECKS PASSED"
else
    echo "$failures JS image check(s) failed"
fi
exit "$failures"
