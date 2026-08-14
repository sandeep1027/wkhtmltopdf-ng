#!/bin/sh
set -eu

# Consumer-application validation for a built .deb.
#
# Usage: validate-consumers.sh DEB BASE_IMAGE
#   DEB        path to the .deb to validate (e.g. dist/wkhtmltopdf-ng_*.deb)
#   BASE_IMAGE distro to run the consumers in (e.g. ubuntu:24.04)
#
# Installs the package in a fresh container, then exercises well-known
# wkhtmltopdf consumers (Python pdfkit, Ruby wicked_pdf) against the
# installed wkhtmltopdf compatibility symlink.

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
deb=${1:-}
base_image=${2:-}

if [ -z "$deb" ] || [ -z "$base_image" ]; then
    printf '%s\n' 'usage: validate-consumers.sh DEB BASE_IMAGE' >&2
    exit 2
fi

if [ ! -f "$deb" ]; then
    printf 'package not found: %s\n' "$deb" >&2
    exit 2
fi

docker_command=${DOCKER:-docker}
if ! command -v "$docker_command" >/dev/null 2>&1; then
    printf '%s\n' "Docker executable not found: $docker_command" >&2
    exit 127
fi

deb_dir=$(CDPATH= cd -- "$(dirname -- "$deb")" && pwd)
deb_file=$(basename -- "$deb")

# Place the .deb in an empty staging dir so /pkg/*.deb expands to it alone.
staging=$(mktemp -d "${TMPDIR:-/tmp}/wkhtmltoxng-cons.XXXXXX")
trap 'rm -rf "$staging"' EXIT
cp "$deb" "$staging/$deb_file"

status=0
"$docker_command" run --rm \
    -v "$staging:/pkg:ro" \
    -v "$root:/workspace:ro" \
    "$base_image" \
    /bin/bash /workspace/packaging/scripts/validate-consumers-inner.sh || status=$?
exit "$status"