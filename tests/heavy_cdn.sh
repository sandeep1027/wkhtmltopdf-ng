#!/bin/bash
set -u

# Heavy 60-page image bench + canvas/CDN font+chart.
# Usage: heavy_cdn.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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
    gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$1" 2>/dev/null
}

# --- heavy 60-page ---
pdf="$out_dir/heavy-bench.pdf"
start=$(date +%s.%N)
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 200 --window-status heavy-done \
        "$data_dir/heavy-bench.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "heavy 60-page bench renders"
else
    elapsed=$(python3 -c "print('%.2f' % ($(date +%s.%N) - $start))")
    pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
    check 0 "heavy 60-page bench renders in ${elapsed}s"
    if [ "$pages" = "60" ]; then check 0 "heavy bench is 60 pages"; else check 1 "heavy bench is 60 pages (got ${pages:-none})"; fi
    compact=$(text_of "$pdf" | tr -d ' \n')
    case "$compact" in *HEAVY-READY*) check 0 "heavy JS ready marker" ;; *) check 1 "heavy JS ready marker" ;; esac
    case "$compact" in *HEAVY-PAGE-01*) check 0 "heavy page 01 marker" ;; *) check 1 "heavy page 01 marker" ;; esac
    last=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -dFirstPage=60 -dLastPage=60 -sOutputFile=- "$pdf" 2>/dev/null | tr -d ' \n')
    case "$last" in *HEAVY-PAGE-60*) check 0 "heavy page 60 marker" ;; *) check 1 "heavy page 60 marker" ;; esac
    echo "TIME: heavy-bench ${elapsed}s pages=$pages size=$(wc -c < "$pdf")"
fi

# --- canvas + CDN ---
pdf2="$out_dir/canvas-cdn.pdf"
start=$(date +%s.%N)
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 400 --window-status canvas-cdn-done \
        --load-media-error-handling ignore \
        "$data_dir/canvas-cdn.html" "$pdf2" >/dev/null 2>&1 || [ ! -s "$pdf2" ]; then
    check 1 "canvas/CDN PDF renders"
else
    elapsed=$(python3 -c "print('%.2f' % ($(date +%s.%N) - $start))")
    check 0 "canvas/CDN PDF renders in ${elapsed}s"
    compact=$(text_of "$pdf2" | tr -d ' \n')
    case "$compact" in *CANVAS-CDN-READY*) check 0 "canvas JS finished" ;; *) check 1 "canvas JS finished" ;; esac
    case "$compact" in *CANVAS-BARS*) check 0 "native canvas bars label" ;; *) check 1 "native canvas bars label" ;; esac
    case "$compact" in *CANVAS-LINE*) check 0 "native canvas line label" ;; *) check 1 "native canvas line label" ;; esac
    case "$compact" in *CDN-FONT-READY*|*CDN-FONT-FALLBACK*)
        check 0 "CDN Roboto ready or fallback (offline ok)" ;;
        *) check 1 "CDN Roboto ready or fallback (offline ok)" ;; esac
    case "$compact" in *CDN-CHART-READY*|*CDN-CHART-SKIP*)
        check 0 "Chart.js CDN ready or skipped" ;;
        *) check 1 "Chart.js CDN ready or skipped" ;; esac
    python3 - "$pdf2" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
sys.exit(0 if data.count(b"/XObject") >= 1 else 1)
PY
    check $? "canvas PDF has painted graphics"
    echo "TIME: canvas-cdn ${elapsed}s pages=$(qpdf --show-npages "$pdf2" 2>/dev/null | tr -d ' \n') size=$(wc -c < "$pdf2")"
    echo "$compact" | python3 -c "import sys; t=sys.stdin.read();
print('  font:', 'READY' if 'CDN-FONT-READY' in t else ('FALLBACK' if 'CDN-FONT-FALLBACK' in t else 'missing'))
print('  chart:', 'READY' if 'CDN-CHART-READY' in t else ('SKIP' if 'CDN-CHART-SKIP' in t else 'missing'))"
fi

if [ "$failures" -eq 0 ]; then echo "ALL HEAVY/CDN CHECKS PASSED"; else echo "$failures heavy/CDN check(s) failed"; fi
exit "$failures"
