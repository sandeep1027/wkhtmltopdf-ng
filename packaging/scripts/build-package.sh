#!/bin/sh
set -eu

target=${1:-}
if [ -z "$target" ]; then
    printf '%s\n' "usage: build-package TARGET" >&2
    exit 2
fi

case "$target" in
    debian13-amd64|debian13-arm64) ;;
    *)
        printf 'unsupported target: %s\n' "$target" >&2
        printf '%s\n' 'Qt 6.8 LTS is required. Enabled package targets are debian13-amd64 and debian13-arm64.' >&2
        exit 2
        ;;
esac

src=/src
build=/tmp/wkhtmltopdf-ng-build
staging=/tmp/wkhtmltopdf-ng-package
version=$(tr -d '[:space:]' < "$src/VERSION")
architecture=$(dpkg --print-architecture)
package_name="wkhtmltopdf-ng_${version}_${architecture}.deb"

rm -rf "$build" "$staging"
mkdir -p "$staging/DEBIAN"

cmake -S "$src" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build "$build" --parallel
DESTDIR="$staging" cmake --install "$build" --strip
ln -s wkhtmltopdf-ng "$staging/usr/local/bin/wkhtmltopdf"
ln -s wkhtmltoimage-ng "$staging/usr/local/bin/wkhtmltoimage"

cat > "$staging/DEBIAN/control" <<EOF
Package: wkhtmltopdf-ng
Version: $version
Section: utils
Priority: optional
Architecture: $architecture
Maintainer: wkhtmltopdf-ng contributors
Homepage: https://github.com/wkhtmltopdf-ng/wkhtmltopdf-ng
Depends: ca-certificates, fonts-dejavu-core, libfontconfig1, libfreetype6, libqt6core6 (>= 6.8), libqt6gui6 (>= 6.8), libqt6network6 (>= 6.8), libqt6positioning6 (>= 6.8), libqt6printsupport6 (>= 6.8), libqt6webchannel6 (>= 6.8), libqt6webenginecore6 (>= 6.8), libqt6webenginecore6-bin (>= 6.8), libqt6webenginewidgets6 (>= 6.8), libqt6widgets6 (>= 6.8), qpdf
Provides: wkhtmltopdf
Conflicts: wkhtmltopdf
Replaces: wkhtmltopdf
Description: modern HTML to PDF converter using Qt WebEngine
 wkhtmltopdf-ng renders HTML with Chromium through Qt WebEngine.
 It supports modern HTML, CSS, and JavaScript.
EOF

cat > "$staging/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
ldconfig
exit 0
EOF
chmod 0755 "$staging/DEBIAN/postinst"

mkdir -p /out
dpkg-deb --build --root-owner-group "$staging" "/out/$package_name"
printf 'created /out/%s\n' "$package_name"

smoke=/tmp/wkhtmltopdf-ng-smoke
rm -rf "$smoke"
mkdir -p "$smoke"
dpkg-deb -x "/out/$package_name" "$smoke"
export LD_LIBRARY_PATH="$smoke/usr/local/lib"
QT_QPA_PLATFORM=offscreen "$smoke/usr/local/bin/wkhtmltopdf-ng" --version
QT_QPA_PLATFORM=offscreen "$smoke/usr/local/bin/wkhtmltoimage-ng" --version
printf 'smoke test passed for %s\n' "$package_name"
