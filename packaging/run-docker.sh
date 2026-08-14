#!/bin/sh
# Run wkhtmltopdf-ng in Docker. Works on CentOS 7 hosts that cannot
# execute the native Qt 6 binary.
#
#   packaging/run-docker.sh --enable-local-file-access in.html out.pdf
#   packaging/run-docker.sh --build   # build image first
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
docker_command=${DOCKER:-docker}
tag=${WKHTMLTOPDF_NG_IMAGE:-wkhtmltopdf-ng:local}

if [ "${1:-}" = "--build" ]; then
    shift
    "$root/packaging/build-image.sh" "$tag"
    if [ "$#" -eq 0 ]; then
        exit 0
    fi
fi

if ! command -v "$docker_command" >/dev/null 2>&1; then
    printf '%s\n' "Docker executable not found: $docker_command" >&2
    exit 127
fi

exec "$docker_command" run --rm \
    -e QT_QPA_PLATFORM=offscreen \
    -e QTWEBENGINE_CHROMIUM_FLAGS='--no-sandbox --disable-gpu' \
    -v "$PWD:/work" -w /work \
    "$tag" "$@"
