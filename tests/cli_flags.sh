#!/bin/bash
set -u

# CLI validation that does not need QtWebEngineProcess.
# Usage: cli_flags.sh BINARY

bin=${1:?binary required}
failures=0

expect_fail() {
    local name=$1
    shift
    if "$bin" "$@" >/dev/null 2>&1; then
        echo "FAIL: $name (expected non-zero exit)"
        failures=$((failures + 1))
    else
        echo "PASS: $name"
    fi
}

expect_fail "copies must be >= 1" --copies 0 in.html out.pdf
expect_fail "invalid load-error-handling" --load-error-handling explode in.html out.pdf
expect_fail "invalid viewport-size" --viewport-size wide in.html out.pdf
expect_fail "negative minimum-font-size" --minimum-font-size -1 in.html out.pdf
expect_fail "negative javascript-delay" --javascript-delay -5 in.html out.pdf
expect_fail "cover without input" cover out.pdf
expect_fail "page without input" page out.pdf
expect_fail "global option after object" in.html --page-size A4 out.pdf
expect_fail "invalid log-level" --log-level debug in.html out.pdf
expect_fail "invalid page-ranges" --page-ranges nope in.html out.pdf
expect_fail "invalid image-dpi" --image-dpi 0 in.html out.pdf
expect_fail "invalid image-quality" --image-quality 140 in.html out.pdf
expect_fail "invalid compress-level" --compress-level 12 in.html out.pdf
expect_fail "negative compress-level" --compress-level -1 in.html out.pdf
expect_fail "invalid header-on" --header-on both in.html out.pdf
expect_fail "invalid footer-on" --footer-on left in.html out.pdf
expect_fail "negative retry" --retry -1 in.html out.pdf
expect_fail "negative timeout" --timeout -5 in.html out.pdf
expect_fail "insert without after-page" --insert-pdf extra.pdf orig.pdf out.pdf
expect_fail "after-page negative" --insert-pdf extra.pdf --after-page -1 orig.pdf out.pdf

if "$bin" --dump-default-toc-xsl | grep -q "xsl:stylesheet"; then
    echo "PASS: dump-default-toc-xsl"
else
    echo "FAIL: dump-default-toc-xsl"
    failures=$((failures + 1))
fi

expect_ok() {
    local name=$1
    shift
    if "$bin" "$@" >/dev/null 2>&1; then
        echo "PASS: $name"
    else
        echo "FAIL: $name (expected zero exit)"
        failures=$((failures + 1))
    fi
}

expect_ok "short options (0.12 layout)" -s A4 -O Landscape -T 10mm -R 10mm -B 10mm -L 10mm -d 96 --dump-default-toc-xsl
expect_ok "short flags -g -q -n -l" -g -q -n -l --dump-default-toc-xsl
expect_ok "short proxy -p" -p http://127.0.0.1:8080 --dump-default-toc-xsl
expect_ok "verbose flag" --verbose --dump-default-toc-xsl
expect_ok "short verbose -v" -v --dump-default-toc-xsl
expect_ok "log-level verbose" --log-level verbose --dump-default-toc-xsl
expect_ok "log-level none" --log-level none --dump-default-toc-xsl
expect_fail "unknown short option" -z in.html out.pdf
expect_fail "short page-size missing value" -s

if "$bin" -H 2>/dev/null | grep -q "Usage:"; then
    echo "PASS: extended-help -H"
else
    echo "FAIL: extended-help -H"
    failures=$((failures + 1))
fi

if "$bin" --extended-help 2>/dev/null | grep -q "\\-v, --verbose"; then
    echo "PASS: help lists -v/--verbose"
else
    echo "FAIL: help lists -v/--verbose"
    failures=$((failures + 1))
fi

if "$bin" -h 2>/dev/null | grep -q "\\-s, --page-size"; then
    echo "PASS: help lists -s/--page-size"
else
    echo "FAIL: help lists -s/--page-size"
    failures=$((failures + 1))
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL CLI FLAG CHECKS PASSED"
else
    echo "$failures CLI flag check(s) failed"
fi
exit "$failures"
