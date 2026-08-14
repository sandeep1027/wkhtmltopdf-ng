#!/bin/sh
# Build a Rocky Linux 9 portable tarball (CentOS / RHEL / Alma 9).
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
out=${1:-$root/dist}
docker_command=${DOCKER:-docker}
tag=wkhtmltopdf-ng-el9

if ! command -v "$docker_command" >/dev/null 2>&1; then
    printf '%s\n' "Docker executable not found: $docker_command" >&2
    exit 127
fi

mkdir -p "$out"
"$docker_command" build -f "$root/packaging/docker/Dockerfile.el9" -t "$tag" "$root"
"$docker_command" run --rm -v "$(CDPATH= cd -- "$out" && pwd):/dist" "$tag"
printf 'EL9 archive(s) in %s\n' "$out"
ls -lh "$out"/wkhtmltopdf-ng_*el9*.tar.gz "$out"/wkhtmltopdf-ng_*.tar.gz 2>/dev/null || true
