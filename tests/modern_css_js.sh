#!/bin/bash
set -u

# Modern CSS + JS fixture: verifies the WebEngine rendering path handles
# ES2020 JavaScript and modern CSS. Tier 1 (Grid, clamp(), aspect-ratio,
# flex gap, optional chaining, ...) must pass on every supported engine
# (Qt 6.2 ships Chromium 90). Tier 2 features each carry their own minimum
# Chromium version (e.g. :has()/container queries need 105, toSorted 110);
# on engines below the minimum the fixture reports UNSUPPORTED, which is
# acceptable as long as nothing fails or goes missing. The release builds
# (Debian 13 / Windows / macOS) ship Qt 6.8 -> Chromium ~120, so tier 2 is
# fully enforced there.
# Usage: modern_css_js.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

if ! command -v gs >/dev/null 2>&1; then
    check 1 "ghostscript (gs) is required for modern CSS/JS checks"
    echo "$failures modern CSS/JS check(s) failed"
    exit "$failures"
fi

pdf="$out_dir/modern-css-js.pdf"
if ! "$bin" --enable-local-file-access --no-stop-slow-scripts \
        --javascript-delay 200 --window-status modern-css-js-done \
        "$data_dir/modern-css-js.html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "modern CSS/JS PDF renders"
    echo "1 modern CSS/JS check(s) failed"
    exit 1
fi
check 0 "modern CSS/JS PDF renders"

text=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$pdf" 2>/dev/null \
    | python3 -c '
import re, sys
t = sys.stdin.read().replace("\ufb01", "fi").replace("\ufb02", "fl")
print(re.sub(r"\s+", "", t))
')

python3 - "$text" <<'PY'
import re
import sys

text = sys.argv[1]
failures = 0

def expect(token, present, label):
    global failures
    if present:
        print("%s:PASS" % label)
    else:
        print("%s:FAIL (missing %s)" % (label, token))
        failures += 1

if "JS-FATAL" in text:
    print("JS_FATAL:FAIL (fixture script threw an exception)")
    sys.exit(1)
print("JS_FATAL:PASS (no script exception)")

expect("MODERN-CSS-JS-READY", "MODERN-CSS-JS-READY" in text, "SCRIPT_COMPLETED")

# Tier 1: ES2020 / Chromium 90 baseline and core modern CSS. Must pass
# everywhere the project builds (Qt 6.2+).
tier1 = {
    "JS-OPTCHAIN": "JS-OPTCHAIN:PASS",
    "JS-NULLISH": "JS-NULLISH:PASS",
    "JS-LOGASSIGN": "JS-LOGASSIGN:PASS",
    "JS-REPLACEALL": "JS-REPLACEALL:PASS",
    "JS-PROMISEANY": "JS-PROMISEANY:PASS",
    "JS-INTL": "JS-INTL:PASS",
    "GRID": "GRID:2COL",
    "CLAMP": "CLAMP:PASS",
    "ASPECT": "ASPECT:PASS",
    "FLEX-GAP": "FLEX-GAP:PASS",
}
for label, token in tier1.items():
    expect(token, token in text, label)

# Engine tier: each tier-2 feature has its own minimum Chromium version.
m = re.search(r"ENGINE:CHROME-(\d+)", text)
if m:
    chrome = int(m.group(1))
    print("ENGINE:CHROME-%d" % chrome)
else:
    chrome = 0
    print("ENGINE:UNKNOWN (cannot determine Chromium version)")

# name -> minimum Chromium version that must support it.
tier2 = {
    "JS-AT": 92,
    "JS-HASOWN": 93,
    "JS-ERRORCAUSE": 93,
    "JS-FINDLAST": 97,
    "JS-CLONE": 98,
    "HAS": 105,
    "CONTAINER": 105,
    "JS-TOSORTED": 110,
    "NESTING": 112,
}
for name, minimum in tier2.items():
    if "%s:SUPPORTED" % name in text or "%s:PASS" % name in text:
        state = "supported"
    elif "%s:UNSUPPORTED" % name in text:
        state = "unsupported"
    elif "%s:FAIL" % name in text:
        state = "failed"
    else:
        state = "missing"
    if chrome >= minimum:
        if state == "supported":
            print("%s:PASS (supported)" % name)
        else:
            print("%s:FAIL (required on Chromium >= %d, got %s)" % (name, minimum, state))
            failures += 1
    elif state == "failed" or state == "missing":
        print("%s:FAIL (feature broken/missing on Chromium %d)" % (name, chrome))
        failures += 1
    else:
        print("%s:PASS (tier-2 on old engine: %s)" % (name, state))

sys.exit(1 if failures else 0)
PY
check $? "modern CSS/JS feature checks"

if [ "$failures" -eq 0 ]; then
    echo "ALL MODERN CSS/JS CHECKS PASSED"
else
    echo "$failures modern CSS/JS check(s) failed"
fi
exit "$failures"
