#!/bin/sh
set -eu

# Clean-install execution validation for a built .deb.
#
# Usage: validate-package.sh DEB BASE_IMAGE
#   DEB        path to the .deb to validate (e.g. dist/wkhtmltopdf-ng_*.deb)
#   BASE_IMAGE distro to install into (e.g. ubuntu:24.04, debian:13)
#
# Runs the package install + render checks inside a fresh container so the
# produced package is exercised exactly as an end user would install it.

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
deb=${1:-}
base_image=${2:-}

if [ -z "$deb" ] || [ -z "$base_image" ]; then
    printf '%s\n' 'usage: validate-package.sh DEB BASE_IMAGE' >&2
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

# Stage the .deb alone so /pkg/*.deb expands to exactly it, avoiding
# unrelated .deb files in the source directory (e.g. dependency debs).
staging=$(mktemp -d "${TMPDIR:-/tmp}/wkhtmltoxng-pkg.XXXXXX")
trap 'rm -rf "$staging"' EXIT
cp "$deb" "$staging/$deb_file"

status=0
"$docker_command" run --rm \
    -v "$staging:/pkg:ro" \
    -v "$root:/workspace:ro" \
    "$base_image" \
    /bin/bash /workspace/packaging/scripts/validate-package-inner.sh || status=$?
exit "$status"
