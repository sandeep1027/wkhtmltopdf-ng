#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
docker_command=${DOCKER:-docker}
target=${1:-}
output_dir=${2:-$root/dist}

if [ -z "$target" ] || [ "$target" = "--help" ] || [ "$target" = "-h" ]; then
    printf '%s\n' 'Usage: packaging/build.sh TARGET [OUTPUT_DIR]'
    printf '%s\n' 'Targets: debian13-amd64, debian13-arm64 (Qt 6.8 LTS)'
    printf '%s\n' 'Set DOCKER_PLATFORM to build for another architecture, e.g.'
    printf '%s\n' '  DOCKER_PLATFORM=linux/arm64 packaging/build.sh debian13-arm64'
    exit 0
fi

case "$target" in
    debian13-amd64|debian13-arm64) base_image=debian:13 ;;
    ubuntu24-amd64|ubuntu24-arm64|ubuntu22-amd64|debian12-amd64|debian11-amd64)
        printf '%s\n' 'This target is disabled: Qt 6.8 LTS is required for page-range printing.' >&2
        exit 2
        ;;
    *) printf 'unknown target: %s\n' "$target" >&2; exit 2 ;;
esac

if ! command -v "$docker_command" >/dev/null 2>&1; then
    printf '%s\n' "Docker executable not found: $docker_command" >&2
    exit 127
fi

mkdir -p "$output_dir"
image="wkhtmltopdf-ng/build:$target"
platform_args=
if [ -n "${DOCKER_PLATFORM:-}" ]; then
    platform_args="--platform $DOCKER_PLATFORM"
fi

set -- $platform_args
if "$docker_command" buildx version >/dev/null 2>&1; then
    "$docker_command" buildx build "$@" \
        --build-arg "BASE_IMAGE=$base_image" \
        -f "$root/packaging/docker/Dockerfile.build" \
        -t "$image" \
        --load "$root"
else
    "$docker_command" build "$@" \
        --build-arg "BASE_IMAGE=$base_image" \
        -f "$root/packaging/docker/Dockerfile.build" \
        -t "$image" "$root"
fi

set -- $platform_args
"$docker_command" run "$@" --rm \
    --user "$(id -u):$(id -g)" \
    -v "$root:/src:ro" \
    -v "$(CDPATH= cd -- "$output_dir" && pwd):/out" \
    "$image" "$target"
