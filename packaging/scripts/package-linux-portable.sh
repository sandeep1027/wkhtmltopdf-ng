#!/bin/sh
# Build a relocatable Linux CLI tree: binaries + Qt WebEngine + qpdf.
# The archive is standalone on the same glibc family it was built on.
# It is not a single static executable (Chromium/WebEngine cannot be).
#
# Usage:
#   packaging/scripts/package-linux-portable.sh [BINARY_DIR] [OUTPUT_DIR]
#
# BINARY_DIR must contain wkhtmltopdf-ng, wkhtmltoimage-ng, and libwkhtmltox.so*

set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
bin_dir=${1:-$root/dist/bin}
out_dir=${2:-$root/dist}
version=$(tr -d '[:space:]' < "$root/VERSION")
arch=$(uname -m)
case "$arch" in
    x86_64) arch=amd64 ;;
    aarch64|arm64) arch=arm64 ;;
esac

# Distinct archive names so an EL9 tarball does not overwrite an Ubuntu one.
os_tag=linux
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    case "${ID:-}" in
        rocky|almalinux|rhel|centos)
            os_tag=el${VERSION_ID%%.*}
            ;;
    esac
fi

need() {
    if [ ! -e "$1" ]; then
        printf 'missing %s\n' "$1" >&2
        exit 1
    fi
}

need "$bin_dir/wkhtmltopdf-ng"
need "$bin_dir/wkhtmltoimage-ng"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT
prefix="$stage/wkhtmltopdf-ng"
mkdir -p "$prefix/bin" "$prefix/lib" "$prefix/libexec" \
    "$prefix/plugins/platforms" "$prefix/plugins/tls" "$prefix/plugins/imageformats" \
    "$prefix/resources" "$prefix/translations/qtwebengine_locales"

cp -a "$bin_dir/wkhtmltopdf-ng" "$prefix/libexec/wkhtmltopdf-ng"
cp -a "$bin_dir/wkhtmltoimage-ng" "$prefix/libexec/wkhtmltoimage-ng"
if ls "$bin_dir"/libwkhtmltox.so* >/dev/null 2>&1; then
    cp -a "$bin_dir"/libwkhtmltox.so* "$prefix/lib/"
fi

qmake_bin=$(command -v qmake6 || command -v qmake-qt6 || command -v qmake || true)
qt_libexec=
qt_data=
qt_plugins=
if [ -n "$qmake_bin" ]; then
    qt_libexec=$("$qmake_bin" -query QT_INSTALL_LIBEXECS 2>/dev/null || true)
    qt_data=$("$qmake_bin" -query QT_INSTALL_DATA 2>/dev/null || true)
    qt_plugins=$("$qmake_bin" -query QT_INSTALL_PLUGINS 2>/dev/null || true)
fi
[ -n "$qt_libexec" ] || qt_libexec=/usr/lib/qt6/libexec
[ -n "$qt_data" ] || qt_data=/usr/share/qt6
[ -n "$qt_plugins" ] || qt_plugins=/usr/lib/x86_64-linux-gnu/qt6/plugins

need "$qt_libexec/QtWebEngineProcess"
cp -a "$qt_libexec/QtWebEngineProcess" "$prefix/libexec/QtWebEngineProcess"

copy_if() {
    if [ -e "$1" ]; then
        mkdir -p "$(dirname "$2")"
        cp -a "$1" "$2"
    fi
}

copy_if "$qt_plugins/platforms/libqoffscreen.so" "$prefix/plugins/platforms/libqoffscreen.so"
copy_if "$qt_plugins/platforms/libqminimal.so" "$prefix/plugins/platforms/libqminimal.so"
for plugin in "$qt_plugins"/tls/*.so; do
    [ -e "$plugin" ] && cp -a "$plugin" "$prefix/plugins/tls/"
done
for plugin in "$qt_plugins"/imageformats/*.so; do
    [ -e "$plugin" ] && cp -a "$plugin" "$prefix/plugins/imageformats/"
done

# Qt 6.6+ on EL9 needs v8_context_snapshot.bin as well as the .pak/icu files.
if [ -d "$qt_data/resources" ]; then
    mkdir -p "$prefix/resources"
    cp -a "$qt_data/resources/." "$prefix/resources/"
fi
if [ -d "$qt_data/translations/qtwebengine_locales" ]; then
    cp -a "$qt_data/translations/qtwebengine_locales/"*.pak \
        "$prefix/translations/qtwebengine_locales/" 2>/dev/null || true
fi

if command -v qpdf >/dev/null 2>&1; then
    cp -a "$(command -v qpdf)" "$prefix/bin/qpdf"
fi

copied=
copy_deps() {
    binary=$1
    ldd "$binary" 2>/dev/null | awk '
        /=>/ { print $3 }
        /^\t\// { print $1 }
    ' | while IFS= read -r lib; do
        [ -n "$lib" ] || continue
        [ -e "$lib" ] || continue
        base=$(basename "$lib")
        case "$base" in
            libc.so*|libm.so*|libdl.so*|libpthread.so*|librt.so*|libresolv.so*|ld-linux*|linux-vdso*)
                continue
                ;;
        esac
        dest="$prefix/lib/$base"
        if [ ! -e "$dest" ]; then
            cp -aL "$lib" "$dest"
            copy_deps "$dest"
        fi
    done
}

copy_deps "$prefix/libexec/wkhtmltopdf-ng"
copy_deps "$prefix/libexec/wkhtmltoimage-ng"
copy_deps "$prefix/libexec/QtWebEngineProcess"
[ -e "$prefix/bin/qpdf" ] && copy_deps "$prefix/bin/qpdf"
for so in "$prefix/plugins"/*/*.so; do
    [ -e "$so" ] && copy_deps "$so"
done

cat > "$prefix/bin/wkhtmltopdf-ng-run" <<'EOF'
#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
tool=${WKHTMLTOPDF_NG_TOOL:-wkhtmltopdf-ng}
export LD_LIBRARY_PATH="$root/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$root/plugins"
export QTWEBENGINEPROCESS_PATH="$root/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="$root/resources"
export QTWEBENGINE_LOCALES_PATH="$root/translations/qtwebengine_locales"
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    export QT_QPA_PLATFORM=offscreen
fi
export QTWEBENGINE_DISABLE_SANDBOX=1
export PATH="$here:$PATH"
exec "$root/libexec/$tool" "$@"
EOF
chmod 0755 "$prefix/bin/wkhtmltopdf-ng-run"

for name in wkhtmltopdf-ng wkhtmltoimage-ng wkhtmltopdf wkhtmltoimage; do
    case "$name" in
        wkhtmltoimage*) tool=wkhtmltoimage-ng ;;
        *) tool=wkhtmltopdf-ng ;;
    esac
    printf '%s\n' '#!/bin/sh' \
        "export WKHTMLTOPDF_NG_TOOL=$tool" \
        'exec "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)/wkhtmltopdf-ng-run" "$@"' \
        > "$prefix/bin/$name"
    chmod 0755 "$prefix/bin/$name"
done

cat > "$prefix/README.txt" <<EOF
wkhtmltopdf-ng $version — portable Linux CLI

This folder is a standalone command-line converter. Unpack it and run:

  bin/wkhtmltopdf input.html output.pdf
  bin/wkhtmltoimage input.html output.png

No display server is required. Compatibility names wkhtmltopdf and
wkhtmltoimage are included.

This is not one static file. Qt WebEngine/Chromium is shipped next to the
binaries. Copy the whole folder; do not copy only bin/wkhtmltopdf.

glibc: this bundle was packed on $(ldd --version | head -1) (${os_tag}-${arch}).
It will run on Linux with that glibc or newer.

  CentOS 7 / RHEL 7          — not supported (glibc 2.17)
  CentOS 8 / Rocky 8         — not supported (use the EL9 archive)
  Rocky 9 / Alma 9 / EL9     — use wkhtmltopdf-ng_*_el9-*.tar.gz
  Ubuntu 22.04+ / Debian 12+ — use wkhtmltopdf-ng_*_linux-*.tar.gz

qpdf is bundled when present on the build machine. Headers that use
[page]/[topage], outlines, and --copies need bin/qpdf.
EOF

mkdir -p "$out_dir"
archive="$out_dir/wkhtmltopdf-ng_${version}_${os_tag}-${arch}.tar.gz"
tar -C "$stage" -czf "$archive" wkhtmltopdf-ng
printf 'created %s\n' "$archive"
printf 'tree %s\n' "$prefix"
