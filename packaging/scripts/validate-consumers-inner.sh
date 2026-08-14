#!/bin/bash
set -u

# Consumer-application validation. Runs inside a fresh container where the
# built .deb is mounted at /pkg. Installs the package, then drives the
# well-known wkhtmltopdf consumers through the installed wkhtmltopdf
# compatibility symlink so we know real-world applications work end to end.

export DEBIAN_FRONTEND=noninteractive
export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox

failures=0

check() {
    if [ "$1" -eq 0 ]; then
        echo "PASS: $2"
    else
        echo "FAIL: $2"
        failures=$((failures + 1))
    fi
}

has_magic() {
    local file=$1
    local hex=$2
    local bytes=$(( ${#hex} / 2 ))
    local first
    first=$(od -An -tx1 -N"$bytes" "$file" 2>/dev/null | tr -d ' \n')
    [ "$first" = "$hex" ]
}

check_magic() {
    if has_magic "$1" "$2"; then
        check 0 "$3"
    else
        check 1 "$3"
    fi
}

echo "== Installing package and consumer tooling =="
apt-get update >/dev/null 2>&1
apt-get install -y /pkg/*.deb >/dev/null 2>&1
check $? "package and dependencies install"

apt-get install -y python3 python3-venv python3-pip ruby >/dev/null 2>&1
check $? "python3 + ruby install"

python3 -m venv /opt/venv >/dev/null 2>&1
/opt/venv/bin/pip install --quiet --disable-pip-version-check pdfkit >/dev/null 2>&1
check $? "pip install pdfkit"

gem install --no-document wicked_pdf >/dev/null 2>&1
check $? "gem install wicked_pdf"

echo "== Python pdfkit consumer =="
cat > /tmp/consumer.html <<'EOF'
<!doctype html>
<html>
<head><meta charset="utf-8"><title>Consumer test</title></head>
<body><h1>Consumer validation</h1><p>Rendered via a third-party library.</p></body>
</html>
EOF

cat > /tmp/pdfkit_test.py <<'PYEOF'
import sys
import pdfkit
pdfkit.from_file('/tmp/consumer.html', '/tmp/pdfkit.pdf')
print('pdfkit wrote /tmp/pdfkit.pdf')
PYEOF
/opt/venv/bin/python /tmp/pdfkit_test.py >/tmp/pdfkit.out 2>&1
check $? "pdfkit renders PDF via wkhtmltopdf symlink"
check_magic /tmp/pdfkit.pdf 25504446 "pdfkit PDF has %PDF magic"

echo "== Ruby wicked_pdf consumer =="
cat > /tmp/wicked_test.rb <<'RUBYEOF'
require 'wicked_pdf'
html = '<!doctype html><html><head><meta charset="utf-8"><title>Wicked</title></head><body><h1>Wicked</h1><p>from Ruby</p></body></html>'
pdf = WickedPdf.new.pdf_from_string(html)
File.binwrite('/tmp/wicked.pdf', pdf)
puts "wicked_pdf wrote #{pdf.bytesize} bytes"
RUBYEOF
ruby /tmp/wicked_test.rb >/tmp/wicked.out 2>&1
check $? "wicked_pdf renders PDF via wkhtmltopdf symlink"
check_magic /tmp/wicked.pdf 25504446 "wicked_pdf PDF has %PDF magic"

if [ "$failures" -eq 0 ]; then
    echo "ALL CONSUMER VALIDATION CHECKS PASSED"
else
    echo "$failures consumer validation check(s) failed"
fi
exit "$failures"