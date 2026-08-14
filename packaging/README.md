# Packaging

This directory packages `wkhtmltopdf-ng` directly from its CMake build. It
does not use the archived upstream Qt 4/qmake packaging flow.

## Debian packages

The enabled Linux targets are Debian 13 (Qt 6.8 LTS, amd64 and arm64).
CMake requires Qt 6.8 for `printToPdf` page ranges. Ubuntu 22.04/24.04 and
Debian 12 ship older WebEngine and are disabled unless a custom Qt 6.8
toolchain is supplied.

```sh
packaging/build.sh debian13-amd64
DOCKER_PLATFORM=linux/arm64 packaging/build.sh debian13-arm64
```

Packages are written to `dist/`. Docker must be installed and the current user
must be allowed to access the Docker daemon. Cross-architecture builds use
`buildx` and require QEMU binfmt registrations when building on a foreign
architecture host.

The target matrix is mirrored in `build.yml`:

- `qt` records the Qt major.minor the distribution ships; `debian13-*` is the
  Qt 6.8 LTS validation target.
- `architecture` records the intended package architecture.

Each produced `.deb` is extracted and its binaries executed with `--version`
inside the build container before it is reported as complete (the smoke test).
Debian 11 does not ship a compatible Qt 6 WebEngine development toolchain.
Its target is kept in `build.yml` as disabled until a separately supplied Qt
6 WebEngine toolchain is available. Building against a newer distribution and
calling it a Debian 11 package would create an invalid glibc compatibility
claim.

## Validation

The `.deb` packages are additionally validated in fresh containers of the
target distribution, exactly as an end user would install them:

- `packaging/scripts/validate-package.sh DEB IMAGE` performs a clean install
  with `apt` (resolving the declared Qt/qpdf dependencies), checks the real
  and compatibility binaries, and renders PDFs and images. It verifies `%PDF`
  and PNG magic bytes, qpdf page counts, the qpdf-dependent header-overlay and
  TOC/outline pipelines, and that rendered images are not blank.

  ```sh
  packaging/scripts/validate-package.sh dist/wkhtmltopdf-ng_*.deb debian:13
  packaging/scripts/validate-package.sh dist/wkhtmltopdf-ng_*.deb ubuntu:24.04
  ```

- `packaging/scripts/validate-consumers.sh DEB IMAGE` exercises the installed
  `wkhtmltopdf` compatibility symlink through the well-known Python `pdfkit`
  and Ruby `wicked_pdf` libraries and checks the produced PDFs.

  ```sh
  packaging/scripts/validate-consumers.sh dist/wkhtmltopdf-ng_*.deb debian:13
  ```

- `tests/regression.sh` (wired into CTest as `wkhtmltopdf-ng-regression`)
  verifies rendered output correctness locally: PDF/TOC page counts, non-blank
  PNG capture, and loading a URL through `--proxy`.

The GitHub Actions workflow runs the clean-install and consumer validations for
the Ubuntu 24.04 and Debian 13 amd64 packages.

## Standalone CLI (CentOS / Windows)

Qt WebEngine cannot be linked into one static file. A “standalone” build is a
folder you copy: the CLI, `QtWebEngineProcess`, Qt libraries, resources, and
`qpdf`. Compatibility names `wkhtmltopdf` and `wkhtmltoimage` are included.

| Target | How to get a CLI |
|---|---|
| Windows 10/11 (64-bit) | `packaging/scripts/package-windows.ps1` or the GitHub `windows-amd64` zip. Unpack and run `bin\wkhtmltopdf.exe`. |
| Rocky / Alma / CentOS Stream **9** | Build the EL9 portable tarball (below). |
| Ubuntu 22.04+ / Debian 12+ | `packaging/scripts/package-linux-portable.sh` on that host. |
| CentOS / RHEL **7** | Not supported. glibc 2.17 cannot run Qt 6 WebEngine. |
| CentOS / RHEL **8** | Not supported by this project’s Qt 6.8 stack. Use EL9 or Windows. |

### Linux portable tarball

From a machine that already has a built `wkhtmltopdf-ng`:

```sh
packaging/scripts/package-linux-portable.sh /tmp/wkhtmltopdf-ng-test dist
tar -tzf dist/wkhtmltopdf-ng_*_linux-amd64.tar.gz | head
./wkhtmltopdf-ng/bin/wkhtmltopdf input.html output.pdf
```

### CentOS / RHEL / Rocky 9 portable tarball

```sh
docker build -f packaging/docker/Dockerfile.el9 -t wkhtmltopdf-ng-el9 .
docker run --rm -v "$PWD/dist:/dist" wkhtmltopdf-ng-el9
```

That writes `dist/wkhtmltopdf-ng_*_el9-amd64.tar.gz` (named separately from
the Ubuntu `*_linux-amd64.tar.gz` bundle). Unpack on Rocky / Alma / CentOS
Stream 9 and run `bin/wkhtmltopdf`. No Qt packages need to be installed on
that host. The EL9 tree is not meant to run on Ubuntu.

Windows portable zip cannot be produced on Linux. Build it on Windows 10/11
with `packaging/scripts/package-windows.ps1`, or download the GitHub Actions
`windows-amd64` artifact.

## Windows package

`packaging/scripts/package-windows.ps1` builds with the MSVC 2022 toolchain
and deploys the Qt 6.8 WebEngine runtime with `windeployqt`. The GitHub
Actions workflow `.github/workflows/windows.yml` installs Qt 6.8 WebEngine via
`aqtinstall` and uploads a `windows-amd64` zip containing `bin/`
(`wkhtmltopdf-ng.exe`, `wkhtmltoimage-ng.exe`, compatibility copies
`wkhtmltopdf.exe`/`wkhtmltoimage.exe`, `libwkhtmltox.dll`) and the deployed
Qt/WebEngine runtime.

The packager downloads a Windows `qpdf` build into `bin/` when GitHub
releases are reachable. Page-number overlays and native bookmarks need that
`qpdf.exe`.

## macOS package

`packaging/scripts/package-macos.sh` is experimental. Qt WebEngine on macOS
locates `QtWebEngineProcess` inside an app bundle, so the CLI tools are
deployed into `wkhtmltopdf-ng.app` with `macdeployqt` and exposed through
symlinks at the zip root. The `.github/workflows/macos.yml` workflow installs
Qt 6.8 WebEngine via `aqtinstall` and uploads a `macos` zip.

`qpdf` is also required on `PATH` for overlays and bookmarks.

## Container image

```sh
packaging/build-image.sh wkhtmltopdf-ng:local
docker run --rm -i wkhtmltopdf-ng:local https://example.com - > example.pdf
```

The image runs as an unprivileged user and uses the offscreen Qt platform.
`libqt6webenginecore6-bin` supplies `QtWebEngineProcess`.

The Debian package also installs `wkhtmltoimage-ng` and the compatibility
command `wkhtmltoimage`.
