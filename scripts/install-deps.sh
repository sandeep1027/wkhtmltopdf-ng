#!/usr/bin/env bash
# Install wkhtmltopdf-ng machine requirements for this OS.
#
# Detects Debian / Ubuntu, Rocky / Alma / RHEL 9, Fedora, and Arch.
# Installs system Qt 6 WebEngine, qpdf, fonts, and (by default) the
# build tools. Does not install the portable tarball.
#
# Usage:
#   sudo ./scripts/install-deps.sh              # runtime + build (default)
#   sudo ./scripts/install-deps.sh --runtime    # run a binary built on this OS
#   sudo ./scripts/install-deps.sh --build      # same as default
#   ./scripts/install-deps.sh --check           # verify only
#   ./scripts/install-deps.sh --dry-run         # print packages, do not install
#
# Minimum: Qt 6.2, qpdf 10.6, CMake 3.16, C++17, glibc 2.35
#           (Ubuntu 22.04+ / Debian 12+). --page-ranges needs Qt 6.8.

set -euo pipefail

mode=build
dry_run=0
check_only=0

usage() {
    sed -n '2,16p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --runtime) mode=runtime ;;
        --build) mode=build ;;
        --check) check_only=1 ;;
        --dry-run) dry_run=1 ;;
        -h|--help) usage 0 ;;
        *)
            printf 'unknown option: %s\n' "$1" >&2
            usage 2
            ;;
    esac
    shift
done

if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
else
    printf 'cannot detect OS: /etc/os-release is missing\n' >&2
    exit 1
fi

os_id=${ID:-}
os_like=${ID_LIKE:-}
os_ver=${VERSION_ID:-0}
os_major=${os_ver%%.*}
os_pretty=${PRETTY_NAME:-$os_id}

major_int() {
    printf '%s' "${1%%.*}" | tr -cd '0-9'
    printf '\n'
}

is_like() {
    # $1 needle  — match ID or a word in ID_LIKE
    case " $os_id $os_like " in
        *" $1 "*) return 0 ;;
        *) return 1 ;;
    esac
}

family=
pkg_manager=
case "$os_id" in
    debian|ubuntu) family=debian; pkg_manager=apt ;;
    raspbian) family=debian; pkg_manager=apt ;;
    linuxmint|pop|elementary|zorin) family=debian; pkg_manager=apt ;;
    rocky|almalinux|rhel|centos) family=el; pkg_manager=dnf ;;
    fedora) family=fedora; pkg_manager=dnf ;;
    arch|manjaro|endeavouros) family=arch; pkg_manager=pacman ;;
    opensuse-leap|opensuse-tumbleweed|sles) family=suse; pkg_manager=zypper ;;
    *)
        if is_like debian || is_like ubuntu; then
            family=debian
            pkg_manager=apt
        elif is_like rhel || is_like fedora || is_like centos; then
            if [ "$(major_int "$os_ver")" -ge 9 ] 2>/dev/null; then
                family=el
            else
                family=fedora
            fi
            pkg_manager=dnf
        elif is_like arch; then
            family=arch
            pkg_manager=pacman
        elif is_like suse; then
            family=suse
            pkg_manager=zypper
        else
            printf 'unsupported OS: %s (%s)\n' "$os_pretty" "$os_id" >&2
            printf 'supported: Ubuntu 22.04+, Debian 12+, Rocky/Alma/RHEL 9, Fedora, Arch\n' >&2
            exit 1
        fi
        ;;
esac

refuse_old() {
    printf 'not supported: %s\n' "$os_pretty" >&2
    printf '%s\n' "$1" >&2
    exit 1
}

case "$family" in
    debian)
        case "$os_id" in
            ubuntu|linuxmint|pop|elementary|zorin)
                if [ "$(major_int "$os_ver")" -lt 22 ]; then
                    refuse_old "need Ubuntu 22.04 or newer (Qt 6.2+). This release has no Qt 6 WebEngine."
                fi
                ;;
            debian|raspbian)
                if [ "$(major_int "$os_ver")" -lt 12 ]; then
                    refuse_old "need Debian 12 or newer (Qt 6.2+). Debian 11 is not supported."
                fi
                ;;
        esac
        ;;
    el)
        if [ "$(major_int "$os_ver")" -lt 9 ]; then
            refuse_old "CentOS / RHEL 7 and 8 are not supported (glibc / Qt 6 WebEngine)."
        fi
        ;;
esac

# Package lists. Runtime is always installed. Build adds compilers and -dev.
runtime_pkgs=()
build_pkgs=()
prep_cmds=()

case "$family" in
    debian)
        runtime_pkgs=(
            ca-certificates
            fonts-dejavu-core
            fonts-liberation
            libfontconfig1
            libfreetype6
            libqt6core6
            libqt6gui6
            libqt6network6
            libqt6positioning6
            libqt6printsupport6
            libqt6webchannel6
            libqt6webenginecore6
            libqt6webenginecore6-bin
            libqt6webenginewidgets6
            libqt6widgets6
            qpdf
            qt6-qpa-plugins
        )
        build_pkgs=(
            cmake
            g++
            ninja-build
            pkg-config
            qt6-base-dev
            qt6-webengine-dev
            qt6-webengine-dev-tools
            libqt6webenginecore6-bin
            qpdf
        )
        ;;
    el|fedora)
        runtime_pkgs=(
            ca-certificates
            dejavu-sans-fonts
            fontconfig
            freetype
            qpdf
            qt6-qtbase
            qt6-qtbase-gui
            qt6-qtwebengine
        )
        build_pkgs=(
            cmake
            gcc-c++
            ninja-build
            pkgconf-pkg-config
            qpdf
            qt6-qtbase-devel
            qt6-qtwebengine-devel
        )
        if [ "$family" = el ]; then
            prep_cmds=(
                "dnf -y install epel-release"
                "dnf -y install dnf-plugins-core"
                "dnf config-manager --set-enabled crb"
            )
        fi
        ;;
    arch)
        runtime_pkgs=(
            ca-certificates
            fontconfig
            freetype2
            qpdf
            qt6-base
            qt6-webengine
            ttf-dejavu
            ttf-liberation
        )
        build_pkgs=(
            base-devel
            cmake
            ninja
            pkgconf
            qpdf
            qt6-base
            qt6-webengine
        )
        ;;
    suse)
        runtime_pkgs=(
            ca-certificates
            dejavu-fonts
            fontconfig
            libfreetype6
            libqt6-qtwebengine
            qpdf
        )
        build_pkgs=(
            cmake
            gcc-c++
            ninja
            pkg-config
            qpdf
            qt6-base-devel
            qt6-webengine-devel
        )
        ;;
esac

wanted=("${runtime_pkgs[@]}")
if [ "$mode" = build ]; then
    wanted+=("${build_pkgs[@]}")
fi

# Unique, keep order.
unique_pkgs=()
seen=" "
for p in "${wanted[@]}"; do
    case "$seen" in
        *" $p "*) ;;
        *)
            unique_pkgs+=("$p")
            seen="$seen$p "
            ;;
    esac
done
wanted=("${unique_pkgs[@]}")

printf 'OS:        %s\n' "$os_pretty"
printf 'family:    %s (%s)\n' "$family" "$pkg_manager"
printf 'mode:      %s\n' "$mode"
printf 'packages:  %s\n' "${wanted[*]}"

run_root() {
    if [ "$dry_run" -eq 1 ]; then
        printf 'dry-run: %s\n' "$*"
        return 0
    fi
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        printf 'need root to install packages (no sudo)\n' >&2
        exit 1
    fi
}

install_packages() {
    local cmd
    for cmd in "${prep_cmds[@]+"${prep_cmds[@]}"}"; do
        # shellcheck disable=SC2086
        run_root $cmd
    done

    case "$pkg_manager" in
        apt)
            export DEBIAN_FRONTEND=noninteractive
            run_root apt-get update
            run_root apt-get install -y --no-install-recommends "${wanted[@]}"
            ;;
        dnf)
            run_root dnf -y install "${wanted[@]}"
            ;;
        pacman)
            run_root pacman -Sy --needed --noconfirm "${wanted[@]}"
            ;;
        zypper)
            run_root zypper --non-interactive refresh
            run_root zypper --non-interactive install --no-recommends "${wanted[@]}"
            ;;
    esac
}

check_cmd() {
    local name=$1
    if command -v "$name" >/dev/null 2>&1; then
        printf '  ok  %s  (%s)\n' "$name" "$("$name" --version 2>/dev/null | head -n1)"
        return 0
    fi
    printf '  MISSING  %s\n' "$name"
    return 1
}

check_file() {
    local path
    for path in "$@"; do
        if [ -e "$path" ]; then
            printf '  ok  %s\n' "$path"
            return 0
        fi
    done
    printf '  MISSING  %s\n' "$*"
    return 1
}

verify() {
    local fail=0
    printf '\nChecking runtime:\n'
    check_cmd qpdf || fail=1
    check_file \
        /usr/lib/qt6/libexec/QtWebEngineProcess \
        /usr/lib64/qt6/libexec/QtWebEngineProcess \
        /usr/lib/x86_64-linux-gnu/qt6/libexec/QtWebEngineProcess \
        /usr/lib/aarch64-linux-gnu/qt6/libexec/QtWebEngineProcess \
        || fail=1
    check_file \
        /usr/lib/x86_64-linux-gnu/qt6/plugins/platforms/libqoffscreen.so \
        /usr/lib/aarch64-linux-gnu/qt6/plugins/platforms/libqoffscreen.so \
        /usr/lib64/qt6/plugins/platforms/libqoffscreen.so \
        /usr/lib/qt6/plugins/platforms/libqoffscreen.so \
        || fail=1

    if [ "$mode" = build ]; then
        printf '\nChecking build tools:\n'
        check_cmd cmake || fail=1
        if command -v g++ >/dev/null 2>&1; then
            check_cmd g++ || fail=1
        elif command -v c++ >/dev/null 2>&1; then
            check_cmd c++ || fail=1
        else
            printf '  MISSING  g++ / c++\n'
            fail=1
        fi
        if command -v ninja >/dev/null 2>&1; then
            printf '  ok  ninja  (%s)\n' "$(ninja --version 2>/dev/null)"
        else
            printf '  note ninja not on PATH (cmake -G Unix Makefiles still works)\n'
        fi
    fi

    if command -v wkhtmltopdf-ng >/dev/null 2>&1; then
        printf '\n  ok  wkhtmltopdf-ng  (%s)\n' "$(wkhtmltopdf-ng --version 2>/dev/null | head -n1)"
    else
        printf '\n  note wkhtmltopdf-ng is not on PATH yet.\n'
        printf '       After deps: cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release\n'
        printf '                   cmake --build build && sudo cmake --install build\n'
    fi

    if [ "$fail" -ne 0 ]; then
        printf '\nSome required pieces are missing.\n' >&2
        return 1
    fi
    printf '\nRequirements look installed for %s.\n' "$os_pretty"
    return 0
}

if [ "$check_only" -eq 1 ]; then
    verify
    exit $?
fi

install_packages

if [ "$dry_run" -eq 1 ]; then
    exit 0
fi

verify
