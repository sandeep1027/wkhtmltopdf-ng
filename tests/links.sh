#!/bin/bash
set -u

# Convert the complex hyperlink fixture and check internal / external PDF links.
# Usage: links.sh BINARY TEST_DATA_DIR OUTPUT_DIR

export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}"
if [ -z "${QTWEBENGINE_CHROMIUM_FLAGS:-}" ]; then
    export QTWEBENGINE_CHROMIUM_FLAGS="--no-sandbox"
fi

bin=${1:?binary required}
data_dir=${2:?test data dir required}
out_dir=${3:?output dir required}
mkdir -p "$out_dir"

failures=0
html="$data_dir/links.html"

check() {
    if [ "$1" -eq 0 ]; then
        echo "PASS: $2"
    else
        echo "FAIL: $2"
        failures=$((failures + 1))
    fi
}

extract_links() {
    python3 - "$1" <<'PY'
import re
import sys

path = sys.argv[1]
data = open(path, "rb").read()
# Flatten common PDF string escapes so URI comparisons stay stable.
text = data.replace(b"\\\n", b"").replace(b"\\(", b"(").replace(b"\\)", b")")
uris = []
for match in re.finditer(rb"/URI\s*\((.*?)\)", text):
    uris.append(match.group(1).decode("latin1", "replace"))
for match in re.finditer(rb"/URI\s*<([0-9A-Fa-f]+)>", text):
    try:
        uris.append(bytes.fromhex(match.group(1).decode("ascii")).decode("latin1", "replace"))
    except ValueError:
        pass
# Chromium also leaves destination names / Dest (name) for some internal jumps.
dests = [m.group(1).decode("latin1", "replace")
         for m in re.finditer(rb"/Dest\s*\((.*?)\)", text)]
print("URI:" + "\nURI:".join(uris))
print("DEST:" + "\nDEST:".join(dests))
print("NAMED_DEST:%d" % len(re.findall(rb"/Dest\s*/wkng-", text)))
print("HAS_EXT_DOCS:%d" % int(b"wkhtmltopdf-ng.test/external-docs" in text))
print("HAS_EXT_STATUS:%d" % int(b"example.com/wkng-status" in text))
print("HAS_EXT_ENGINE:%d" % int(b"wkhtmltopdf-ng.test/engine" in text))
print("HAS_MAILTO:%d" % int(b"docs@wkhtmltopdf-ng.test" in text))
print("HAS_APPENDIX:%d" % int(b"/Dest /wkng-appendix" in text or b"/Dest/wkng-appendix" in text))
print("HAS_CATALOG:%d" % int(b"/Dest /wkng-catalog" in text or b"/Dest/wkng-catalog" in text))
print("HAS_OVERVIEW:%d" % int(b"/Dest /wkng-overview" in text or b"/Dest/wkng-overview" in text))
PY
}

has_field() {
    local dump=$1
    local key=$2
    grep -q "^${key}:1$" <<<"$dump"
}

convert() {
    local name=$1
    shift
    local out="$out_dir/$name.pdf"
    if "$bin" --javascript-delay 0 --enable-local-file-access "$@" "$html" "$out" >/dev/null 2>&1 \
        && [ -s "$out" ]; then
        check 0 "$name renders"
        return 0
    fi
    check 1 "$name renders"
    return 1
}

convert links
default_ok=$?
convert links-no-external --disable-external-links
no_ext_ok=$?
convert links-no-internal --disable-internal-links
no_int_ok=$?
convert links-no-links --disable-external-links --disable-internal-links
no_all_ok=$?

if [ "$default_ok" -eq 0 ]; then
    dump=$(extract_links "$out_dir/links.pdf")
    has_field "$dump" HAS_EXT_DOCS; check $? "default PDF keeps https://wkhtmltopdf-ng.test/external-docs"
    has_field "$dump" HAS_EXT_STATUS; check $? "default PDF keeps https://example.com/wkng-status"
    has_field "$dump" HAS_MAILTO; check $? "default PDF keeps mailto:docs@wkhtmltopdf-ng.test"
    has_field "$dump" HAS_APPENDIX; check $? "default PDF keeps internal dest wkng-appendix"
    has_field "$dump" HAS_CATALOG; check $? "default PDF keeps internal dest wkng-catalog"
    echo "$dump" | sed 's/^/  /'
fi

if [ "$no_ext_ok" -eq 0 ]; then
    dump=$(extract_links "$out_dir/links-no-external.pdf")
    if has_field "$dump" HAS_EXT_DOCS || has_field "$dump" HAS_EXT_STATUS || has_field "$dump" HAS_EXT_ENGINE; then
        check 1 "--disable-external-links removes remote URLs"
    else
        check 0 "--disable-external-links removes remote URLs"
    fi
    if has_field "$dump" HAS_MAILTO; then
        check 1 "--disable-external-links removes mailto"
    else
        check 0 "--disable-external-links removes mailto"
    fi
    has_field "$dump" HAS_APPENDIX; check $? "--disable-external-links keeps internal dest wkng-appendix"
    has_field "$dump" HAS_CATALOG; check $? "--disable-external-links keeps internal dest wkng-catalog"
fi

if [ "$no_int_ok" -eq 0 ]; then
    dump=$(extract_links "$out_dir/links-no-internal.pdf")
    has_field "$dump" HAS_EXT_DOCS; check $? "--disable-internal-links keeps remote URLs"
    if has_field "$dump" HAS_APPENDIX || has_field "$dump" HAS_CATALOG || has_field "$dump" HAS_OVERVIEW; then
        check 1 "--disable-internal-links removes in-document Dest annotations"
    else
        check 0 "--disable-internal-links removes in-document Dest annotations"
    fi
fi

if [ "$no_all_ok" -eq 0 ]; then
    dump=$(extract_links "$out_dir/links-no-links.pdf")
    if has_field "$dump" HAS_EXT_DOCS || has_field "$dump" HAS_EXT_STATUS || has_field "$dump" HAS_MAILTO; then
        check 1 "both disable flags remove remote URLs"
    else
        check 0 "both disable flags remove remote URLs"
    fi
    if has_field "$dump" HAS_APPENDIX || has_field "$dump" HAS_CATALOG || has_field "$dump" HAS_OVERVIEW; then
        check 1 "both disable flags remove in-document Dest annotations"
    else
        check 0 "both disable flags remove in-document Dest annotations"
    fi
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL LINK CHECKS PASSED"
else
    echo "$failures link check(s) failed"
fi
exit "$failures"
