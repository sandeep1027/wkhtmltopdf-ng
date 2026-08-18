#!/bin/bash
set -u

# Next-fix benchmark: remote/local/data images, local WOFF2, repeating thead.
# Usage: reg_next.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

pdf="$out_dir/reg.pdf"
if ! "$bin" --enable-local-file-access --window-status reg-ready --compress \
        "$data_dir/reg.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "reg.pdf renders"
    echo "1 reg-next check(s) failed"
    exit 1
fi
check 0 "reg.pdf renders"

pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
if [ "${pages:-0}" -ge 4 ]; then
    check 0 "table spans extra pages ($pages pages)"
else
    check 1 "table spans extra pages (got ${pages:-none})"
fi

text=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$pdf" 2>/dev/null)
compact=$(printf '%s' "$text" | tr -d ' \n')

case "$compact" in *IMG-LOCALOKnw=80*|*IMG-LOCALOKnw=*) check 0 "local file image loads" ;;
    *) check 1 "local file image loads" ;; esac
case "$compact" in *IMG-DATAOKnw=*) check 0 "data URI image loads" ;;
    *) check 1 "data URI image loads" ;; esac
case "$compact" in *FONTWOFF2-READY*) check 0 "local WOFF2 @font-face ready" ;;
    *) check 1 "local WOFF2 @font-face ready" ;; esac
case "$compact" in *WOFF2-LOCAL*) check 0 "WOFF2 sample text present" ;;
    *) check 1 "WOFF2 sample text present" ;; esac

# Unsplash is network-dependent; report but do not fail the suite.
case "$compact" in
    *REMOTEOK*) echo "PASS: remote Unsplash images loaded (network)" ;;
    *) echo "SKIP: remote Unsplash images not loaded (network, not a renderer fail)" ;;
esac

last=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite \
    -dFirstPage="$pages" -dLastPage="$pages" -sOutputFile=- "$pdf" 2>/dev/null | tr -d ' \n')
case "$last" in
    *RowCodeNameAmount*) check 0 "thead repeats on last table page" ;;
    *) check 1 "thead repeats on last table page" ;;
esac

if [ "$failures" -eq 0 ]; then
    echo "ALL REG-NEXT CHECKS PASSED"
else
    echo "$failures reg-next check(s) failed"
fi
exit "$failures"
