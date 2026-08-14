# wkhtmltopdf-ng

HTML to PDF and HTML to image CLI, built on **Qt 6 WebEngine** (Chromium).
It is a modern replacement for archived [wkhtmltopdf](https://wkhtmltopdf.org/)
(Qt WebKit).

Version **0.13.1**. License: LGPL-3.0.

Full flag list, packaging, limits, and tests: [`docs/DOC.txt`](docs/DOC.txt).

## What you get

- `wkhtmltopdf-ng` — HTML/URL/stdin → PDF
- `wkhtmltoimage-ng` — HTML/URL → PNG, JPEG, WebP
- `libwkhtmltox` — C API with 0.12-style `wkhtmltopdf_*` / `wkhtmltoimage_*` names
- Compatibility names `wkhtmltopdf` and `wkhtmltoimage` in portable trees

Modern CSS and JavaScript work (flex, grid, `@media print`, canvas, webfonts).
Page numbers, copies, outlines, and compression use bundled **qpdf**.

## Quick start

```sh
wkhtmltopdf-ng https://example.com out.pdf
wkhtmltopdf-ng --enable-local-file-access --page-size A4 input.html out.pdf
wkhtmltopdf-ng --header-right "[page]/[topage]" --footer-center "Page [page]" \
  --enable-local-file-access input.html out.pdf
wkhtmltoimage-ng --width 1200 input.html shot.png
wkhtmltopdf-ng --insert-pdf extra.pdf --after-page 3 original.pdf out.pdf
wkhtmltopdf-ng --merge-pdf a.pdf b.pdf combined.pdf
wkhtmltopdf-ng --split-pdf --page-ranges 1-2 original.pdf first-two.pdf
```

Headless / no display:

```sh
export QT_QPA_PLATFORM=offscreen
export QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox
```

`--help` prints the full option list.

## Build from source

Needs Qt **6.2+** with WebEngine (page ranges need **6.8+**), `qpdf`, CMake, C++17.

Debian 13:

```sh
sudo apt install qt6-base-dev qt6-webengine-dev qt6-webengine-dev-tools \
  libqt6webenginecore6-bin qpdf cmake ninja-build
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

GitHub Releases: https://github.com/sandeep1027/wkhtmltopdf-ng/releases

## Portable builds (copy to another machine)

Qt WebEngine cannot be one static file. Unpack the folder and run `bin/wkhtmltopdf`.
Archives are `.tar.xz` (about 100–130 MB). Chromium itself is ~160 MB, so they
cannot shrink to a few megabytes.

| Other machine | Archive / how |
|---|---|
| Ubuntu 22.04+ / Debian 12+ | `packaging/scripts/package-linux-portable.sh` → `*_linux-amd64.tar.xz` |
| Rocky / Alma / CentOS Stream **9** | `packaging/scripts/package-el9.sh dist` → `*_el9-amd64.tar.xz` |
| Windows 10/11 | `packaging/scripts/package-windows.ps1` or the Actions `windows-amd64` zip |
| CentOS / RHEL **7** | Not supported (glibc 2.17). Use Docker. |

```sh
tar -xJf wkhtmltopdf-ng_0.13.1_linux-amd64.tar.xz   # or the el9 file
./wkhtmltopdf-ng/bin/wkhtmltopdf --version
./wkhtmltopdf-ng/bin/wkhtmltopdf --enable-local-file-access in.html out.pdf
```

CentOS 7 via Docker:

```sh
packaging/run-docker.sh --build
packaging/run-docker.sh --enable-local-file-access in.html out.pdf
```

See [`packaging/README.md`](packaging/README.md).

## Limits vs original wkhtmltopdf

- Not a true static one-file binary
- No native AcroForms or PDF/A
- Headers/footers and `[page]`/`[topage]` are a **qpdf overlay**, not patched WebKit bands
- `--page-ranges` needs Qt 6.8+
- Will not run on CentOS 7 even if you swap glibc or use patchelf

## Repository

```
src/           CLI, converter, C API
tests/         fixtures and shell checks
packaging/     Debian, Docker, Linux/Windows/macOS portable
docs/DOC.txt   full documentation
```
