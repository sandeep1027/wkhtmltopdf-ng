#!/bin/bash
set -eu

# Experimental macOS packaging.
#
# Qt WebEngine on macOS expects QtWebEngineProcess inside an app bundle
# (.app/Helpers/QtWebEngineProcess), so the CLI tools are wrapped in a
# minimal bundle and deployed with macdeployqt. Users can invoke the
# executables directly via the symlinks in the zip root.
#
# qpdf must be installed separately and reachable via PATH for PDF
# page-number overlays and native bookmarks to work.

QtRoot="${1:?usage: package-macos.sh QTDIR SOURCE [OUTPUT_DIR]}"
Source="${2:?usage: package-macos.sh QTDIR SOURCE [OUTPUT_DIR]}"
Output="${3:-$PWD/dist}"

build_dir="$(mktemp -d)"
stage_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir" "$stage_dir"' EXIT

cmake -S "$Source" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QtRoot"
cmake --build "$build_dir" --parallel
DESTDIR="$stage_dir" cmake --install "$build_dir" --prefix "$stage_dir/prefix"

app="$stage_dir/wkhtmltopdf-ng.app"
mkdir -p "$app/Contents/MacOS"
cp "$stage_dir/prefix/bin/wkhtmltopdf-ng" "$app/Contents/MacOS/wkhtmltopdf-ng"
cp "$stage_dir/prefix/bin/wkhtmltoimage-ng" "$app/Contents/MacOS/wkhtmltoimage-ng"
cp "$stage_dir/prefix/lib/libwkhtmltox"*.dylib "$app/Contents/MacOS/"

"$QtRoot/bin/macdeployqt" "$app" \
    -executable="$app/Contents/MacOS/wkhtmltopdf-ng" \
    -executable="$app/Contents/MacOS/wkhtmltoimage-ng"

ln -s "wkhtmltopdf-ng.app/Contents/MacOS/wkhtmltopdf-ng" "$stage_dir/wkhtmltopdf-ng"
ln -s "wkhtmltopdf-ng.app/Contents/MacOS/wkhtmltopdf-ng" "$stage_dir/wkhtmltopdf"
ln -s "wkhtmltopdf-ng.app/Contents/MacOS/wkhtmltoimage-ng" "$stage_dir/wkhtmltoimage-ng"
ln -s "wkhtmltopdf-ng.app/Contents/MacOS/wkhtmltoimage-ng" "$stage_dir/wkhtmltoimage"
cp "$stage_dir/prefix/include/wkhtmltox/wkhtmltox.h" "$stage_dir/wkhtmltox.h"

mkdir -p "$Output"
version="$(tr -d '[:space:]' < "$Source/VERSION")"
zip="$Output/wkhtmltopdf-ng_${version}_macos.zip"
rm -f "$zip"
(cd "$stage_dir" && zip -qr "$zip" .)
echo "created $zip"
