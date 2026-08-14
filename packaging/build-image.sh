#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
base_image=${BASE_IMAGE:-debian:13}
tag=${1:-wkhtmltopdf-ng:local}
docker_command=${DOCKER:-docker}

if ! command -v "$docker_command" >/dev/null 2>&1; then
    printf '%s\n' "Docker executable not found: $docker_command" >&2
    exit 127
fi

"$docker_command" build \
    --build-arg "BASE_IMAGE=$base_image" \
    -f "$root/packaging/docker/Dockerfile.image" \
    -t "$tag" "$root"
