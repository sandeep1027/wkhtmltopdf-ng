# wkhtmltopdf-ng

`wkhtmltopdf-ng` is a Qt WebEngine based HTML-to-PDF command-line converter.
It uses Chromium through Qt WebEngine instead of the archived Qt WebKit engine
used by upstream wkhtmltopdf.

## Status

Phase 1 currently provides:

- URL, local-file, and stdin HTML input
- PDF file and stdout output
- Modern JavaScript and CSS through Qt WebEngine
- Page size, orientation, margins, title, zoom, and JavaScript delay (default 200 ms)
- Layout `--dpi` (scales content by `96/dpi`, combined with `--zoom`)
- Smart shrinking: shrink content that is wider than the printable page
- `--viewport-size`, `--minimum-font-size`, `--background` / `--no-background`
- `--load-error-handling` abort/ignore/skip, `--copies`, `--no-pdf-compression`
- `--image-dpi`, `--image-quality`, and `--lowquality` (downsample page images before print)
- Document objects: `page`, `cover`, and `toc`, with per-object page options
- `--cookie`, `--allow`, `--cache-dir`, SOCKS/bypass proxy, repeatable `--post`
- Header tokens including `[frompage]`, `[section]`, `[isodate]`, `[doctitle]`
- Nested `--dump-outline` XML, TOC back-links, `--toc-xsl` via `xsltproc`
- `wkhtmltoimage-ng` shares `HtmlToImageConverter` with the C API
- Local-file access control and custom request headers
- Static CSS-based text and HTML headers/footers
- Generated heading table of contents with links and dotted leaders
- Heading outline XML export with `--dump-outline`
- Accurate `[page]` and `[topage]` overlays using the rendered PDF page count
- `--page-ranges` via Qt 6.8 `printToPdf` (`1-3,5`)
- Multiple input documents combined with page breaks
- Relative document resources normalized per input during multi-document conversion
- `wkhtmltoimage-ng` PNG, JPEG, and WebP screenshots
- HTTP/HTTPS proxy and proxy authentication
- A source-compatible `libwkhtmltox` C API with the PDF (`wkhtmltopdf_*`) and
  image (`wkhtmltoimage_*`) symbol surface, including multi-phase progress and
  error callbacks

The Qt WebEngine print API does not expose all of wkhtmltopdf's patched WebKit
features. In particular, PDF bookmark embedding and resource types outside the
common URL-bearing attributes and CSS URL cases still need work.
They are not silently presented as complete compatibility.

## Build

Qt **6.8 LTS** or newer with WebEngine development files, the WebEngine process
runtime, `qpdf`, CMake, and a C++17 compiler are required. Page-range printing
uses the Qt 6.8 `QWebEnginePage::printToPdf` API.

On Debian 13 (Qt 6.8 LTS), the relevant packages are:

```sh
sudo apt install qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
  libqt6webenginecore6-bin qpdf cmake ninja-build
```

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Usage

```sh
wkhtmltopdf-ng https://example.com example.pdf
wkhtmltopdf-ng --page-size A4 --margin-top 20mm --page-ranges 1-2 input.html output.pdf
wkhtmltopdf-ng --enable-local-file-access --margin-top 22mm --margin-bottom 22mm \
  --header-html tests/test_data/pagination-header.html \
  --footer-html tests/test_data/pagination-footer.html \
  tests/test_data/pagination.html pagination.pdf
wkhtmltopdf-ng --enable-local-file-access \
  --header-left "Report" --header-right "[page]/[topage]" \
  --footer-center "Page [page] of [topage]" \
  tests/test_data/pagination.html pagination-tokens.pdf
cat input.html | wkhtmltopdf-ng --enable-local-file-access - output.pdf
wkhtmltopdf-ng https://example.com - > example.pdf
wkhtmltoimage-ng https://example.com screenshot.png
wkhtmltoimage-ng --width 1200 --quality 85 --crop-width 800 input.html crop.png
```

On Linux servers without a display, use the Qt platform plugin supported by
the deployment environment, commonly `QT_QPA_PLATFORM=offscreen`.

Standalone CLI trees (Windows zip, Linux tarball, Rocky/CentOS 9 image) are
documented in `packaging/README.md`. Qt WebEngine cannot ship as one static
file; the portable archive is a folder you copy and run from `bin/`.

CentOS 7 cannot run the native binary. Use Docker:

```sh
packaging/run-docker.sh --build
packaging/run-docker.sh --enable-local-file-access in.html out.pdf
```

Rocky / Alma / CentOS Stream 9 portable tarball:

```sh
packaging/scripts/package-el9.sh dist
# writes dist/wkhtmltopdf-ng_*_el9-amd64.tar.gz
```

## C API

The installed header is `wkhtmltox/wkhtmltox.h` and the shared library is
`libwkhtmltox`. The exported PDF API names and callback signatures follow
wkhtmltopdf 0.12.x. Both converter families are exported:

- `wkhtmltopdf_*` - PDF conversion with `wkhtmltopdf_add_object`,
  `wkhtmltopdf_convert`, `wkhtmltopdf_get_output`, and the standard six-phase
  progress model ("Loading pages" ... "Done").
- `wkhtmltoimage_*` - image conversion driven by `wkhtmltoimage_set_global_setting`
  (`in`, `out`, `fmt`, `screenWidth`, `screenHeight`, `quality`, `crop.*`,
  `transparent`, `enableJavascript`, `javascriptDelay`, `enableLocalFileAccess`)
  and the three-phase progress model ("Loading page", "Rendering page", "Done").

The library creates its own `QApplication` when the host process has not done
so, so in-process consumers (e.g. Python/Ruby bindings) do not need to
construct a Qt application. ABI compatibility must still be validated against
each consumer and platform before replacing an existing binary. The CTest suite
includes a `wkhtmltox-capi-symbols` check for symbol presence and phase
descriptions, plus a render check (`wkhtmltox-capi-convert`) when the
WebEngine runtime is available.
