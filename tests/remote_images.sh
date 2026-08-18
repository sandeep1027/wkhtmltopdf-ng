#!/bin/bash
set -u

# file:// HTML must load remote http(s) images without --enable-local-file-access
# (wkhtmltopdf 0.12 behaviour). Local sibling files stay blocked.
# Usage: remote_images.sh BINARY TEST_DATA_DIR OUTPUT_DIR

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

asset_dir="$data_dir/assets"
if [ ! -f "$asset_dir/photo-red.png" ]; then
    check 1 "test asset photo-red.png exists"
    exit 1
fi

port_file="$out_dir/http.port"
python3 - "$asset_dir" "$port_file" <<'PY' &
import os, sys
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer

root, port_file = sys.argv[1], sys.argv[2]
os.chdir(root)
httpd = ThreadingHTTPServer(("127.0.0.1", 0), SimpleHTTPRequestHandler)
with open(port_file, "w") as fh:
    fh.write(str(httpd.server_address[1]))
httpd.serve_forever()
PY
server_pid=$!
cleanup() { kill "$server_pid" >/dev/null 2>&1 || true; }
trap cleanup EXIT

port=""
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if [ -s "$port_file" ]; then
        port=$(tr -d ' \n' < "$port_file")
        break
    fi
    sleep 0.1
done
if [ -z "$port" ]; then
    check 1 "local HTTP server started"
    exit 1
fi

cp "$asset_dir/photo-red.png" "$out_dir/photo-red.png"
html="$out_dir/remote-img.html"
cat > "$html" <<HTML
<!doctype html>
<html><head><meta charset="utf-8"><title>remote</title></head>
<body>
<img id="remote" src="http://127.0.0.1:${port}/photo-red.png" alt="remote">
<img id="local" src="photo-red.png" alt="local">
<p id="st">pending</p>
<script>
function report() {
  var remote = document.getElementById("remote");
  var local = document.getElementById("local");
  var remoteOk = remote.complete && remote.naturalWidth > 0;
  var localOk = local.complete && local.naturalWidth > 0;
  var line = "REMOTE:" + (remoteOk ? "OK" : "FAIL") +
             " LOCAL:" + (localOk ? "OK" : "FAIL");
  document.getElementById("st").textContent = line;
  window.status = remoteOk ? "remote-img-ok" : "remote-img-fail";
}
window.addEventListener("load", function() {
  setTimeout(report, 50);
});
</script>
</body></html>
HTML

pdf="$out_dir/remote-img.pdf"
if ! "$bin" --javascript-delay 1500 --no-stop-slow-scripts \
        --window-status remote-img-ok \
        --load-media-error-handling ignore \
        "$html" "$pdf" >/dev/null 2>&1 || [ ! -s "$pdf" ]; then
    check 1 "remote image loads without --enable-local-file-access"
else
    check 0 "remote image loads without --enable-local-file-access"
fi

if [ -s "$pdf" ]; then
    text=$(gs -q -dNOPAUSE -dBATCH -sDEVICE=txtwrite -sOutputFile=- "$pdf" 2>/dev/null | tr -d ' \n')
    case "$text" in
        *REMOTE:OK*) check 0 "PDF text reports REMOTE:OK" ;;
        *) check 1 "PDF text reports REMOTE:OK (got ${text:-empty})" ;;
    esac
    case "$text" in
        *LOCAL:FAIL*|*LOCAL:OK*)
            case "$text" in
                *LOCAL:FAIL*) check 0 "local sibling image still blocked" ;;
                *) check 1 "local sibling image still blocked (got LOCAL:OK)" ;;
            esac
            ;;
        *) check 1 "PDF reports local image status" ;;
    esac
fi

if [ "$failures" -eq 0 ]; then
    echo "ALL REMOTE IMAGE CHECKS PASSED"
else
    echo "$failures remote image check(s) failed"
fi
exit "$failures"
