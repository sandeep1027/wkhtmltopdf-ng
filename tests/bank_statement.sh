#!/bin/bash
set -u

# Bank-statement fixture: images, hrefs, and checkboxes.
# Usage: bank_statement.sh BINARY TEST_DATA_DIR OUTPUT_DIR

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

bin=${1:?binary required}
data_dir=${2:?test data dir required}
out_dir=${3:?output dir required}
mkdir -p "$out_dir"

failures=0
pdf="$out_dir/bank-statement.pdf"

check() {
    if [ "$1" -eq 0 ]; then
        echo "PASS: $2"
    else
        echo "FAIL: $2"
        failures=$((failures + 1))
    fi
}

if ! "$bin" --javascript-delay 0 --enable-local-file-access \
        --margin-top 12mm --margin-bottom 14mm \
        --header-center "Northridge Bank · [page]/[topage]" \
        --footer-left "STMT-2026-07-4412" \
        --footer-right "Page [page] of [topage]" \
        "$data_dir/bank-statement.html" "$pdf" >/dev/null 2>&1 \
        || [ ! -s "$pdf" ]; then
    check 1 "bank statement PDF renders"
    echo "1 bank statement check(s) failed"
    exit 1
fi
check 0 "bank statement PDF renders"

if command -v qpdf >/dev/null 2>&1; then
    pages=$(qpdf --show-npages "$pdf" 2>/dev/null | tr -d ' \n')
    if [ "${pages:-0}" -ge 2 ]; then
        check 0 "bank statement spans at least 2 pages ($pages)"
    else
        check 1 "bank statement spans at least 2 pages (got ${pages:-none})"
    fi
fi

python3 - "$pdf" <<'PY'
import re
import sys

data = open(sys.argv[1], "rb").read()
failures = 0

def ok(cond, name):
    global failures
    print(("PASS: " if cond else "FAIL: ") + name)
    if not cond:
        failures += 1

uris = [m.group(1).decode("latin1", "replace")
        for m in re.finditer(rb"/URI\s*\((.*?)\)", data)]
joined = "\n".join(uris)
ok(any("northridge-bank.test" in u for u in uris),
   "statement identity present")
ok("https://northridge-bank.test/online" in joined, "logo/online href is a PDF URI")
ok("https://northridge-bank.test/support" in joined, "support href is a PDF URI")
ok("https://northridge-bank.test/disputes" in joined, "disputes href is a PDF URI")
ok("https://northridge-bank.test/receipts/PR-8831" in joined, "receipt href is a PDF URI")
ok("mailto:statements@northridge-bank.test" in joined, "mailto href is a PDF URI")
ok(b"/Dest /summary" in data or b"/Dest /activity" in data or b"/Dest /disputes" in data,
   "internal statement anchors exist")
ok(data.count(b"/XObject") >= 1,
   "embedded images/graphics present")
sys.exit(1 if failures else 0)
PY
check $? "images and hyperlinks are embedded"

if command -v gs >/dev/null 2>&1; then
    text=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$pdf" 2>/dev/null \
        | python3 -c 'import sys; print(sys.stdin.read().replace("\ufb01","fi"))')
    compact=$(printf '%s' "$text" | tr -d ' \n')
    case "$compact" in *BANKSTMT-OK*) check 0 "visible BANKSTMT-OK marker" ;;
        *) check 1 "visible BANKSTMT-OK marker" ;; esac
    case "$compact" in *BANK-CHECKBOX-PANEL*) check 0 "checkbox preference panel text" ;;
        *) check 1 "checkbox preference panel text" ;; esac
    case "$compact" in *BANK-LEDGER*) check 0 "ledger marker" ;;
        *) check 1 "ledger marker" ;; esac
    case "$compact" in *BANK-DISPUTE-HOLD*) check 0 "dispute worksheet text" ;;
        *) check 1 "dispute worksheet text" ;; esac
    case "$compact" in *Paperlesse-statement*|*Paperless*) check 0 "checkbox label Paperless e-statement" ;;
        *) check 1 "checkbox label Paperless e-statement" ;; esac
    case "$compact" in *AlexRivera*) check 0 "account holder name" ;;
        *) check 1 "account holder name" ;; esac
    case "$compact" in *2,450.00*|*2450.00*|*2,450*) check 0 "payroll credit amount" ;;
        *) check 1 "payroll credit amount" ;; esac
else
    check 0 "ghostscript text extract (gs unavailable, skipped)"
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL BANK STATEMENT CHECKS PASSED"
else
    echo "$failures bank statement check(s) failed"
fi
exit "$failures"
