param(
    [Parameter(Mandatory = $true)][string]$QtRoot,
    [Parameter(Mandatory = $true)][string]$Source,
    [Parameter(Mandatory = $false)][string]$Output = "dist"
)

$ErrorActionPreference = "Stop"

$buildDir = Join-Path $env:TEMP "wkhtmltopdf-ng-build"
$stageDir = Join-Path $env:TEMP "wkhtmltopdf-ng-stage"

if (Test-Path $buildDir) { Remove-Item -Recurse -Force $buildDir }
if (Test-Path $stageDir) { Remove-Item -Recurse -Force $stageDir }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Write-Host "Configuring with Qt at $QtRoot"
cmake -S $Source -B $buildDir -G "Visual Studio 17 2022" -A x64 `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_PREFIX_PATH="$QtRoot"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building"
cmake --build $buildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Installing to staging"
cmake --install $buildDir --config Release --prefix $stageDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$binDir = Join-Path $stageDir "bin"
Copy-Item (Join-Path $binDir "wkhtmltopdf-ng.exe") (Join-Path $binDir "wkhtmltopdf.exe")
Copy-Item (Join-Path $binDir "wkhtmltoimage-ng.exe") (Join-Path $binDir "wkhtmltoimage.exe")

$windeployqt = Join-Path $QtRoot "bin/windeployqt.exe"
foreach ($exe in @("wkhtmltopdf-ng.exe", "wkhtmltoimage-ng.exe", "wkhtmltopdf.exe", "wkhtmltoimage.exe")) {
    Write-Host "Running windeployqt on $exe"
    & $windeployqt --release --no-translations (Join-Path $binDir $exe)
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$qpdfVersion = "11.9.1"
$qpdfZip = Join-Path $env:TEMP "qpdf-$qpdfVersion-msvc64.zip"
$qpdfUrl = "https://github.com/qpdf/qpdf/releases/download/v$qpdfVersion/qpdf-$qpdfVersion-msvc64.zip"
try {
    Write-Host "Downloading qpdf $qpdfVersion"
    Invoke-WebRequest -Uri $qpdfUrl -OutFile $qpdfZip
    $qpdfExtract = Join-Path $env:TEMP "qpdf-$qpdfVersion"
    if (Test-Path $qpdfExtract) { Remove-Item -Recurse -Force $qpdfExtract }
    Expand-Archive -Path $qpdfZip -DestinationPath $qpdfExtract -Force
    $qpdfBin = Get-ChildItem -Path $qpdfExtract -Recurse -Filter "qpdf.exe" | Select-Object -First 1
    if ($qpdfBin) {
        Copy-Item $qpdfBin.FullName (Join-Path $binDir "qpdf.exe")
        Get-ChildItem $qpdfBin.Directory -Filter "*.dll" | ForEach-Object {
            Copy-Item $_.FullName $binDir -Force
        }
        Write-Host "bundled $($qpdfBin.FullName)"
    }
} catch {
    Write-Warning "qpdf was not bundled: $_"
    Write-Warning "Install qpdf and put it on PATH for [page]/[topage] headers and outlines."
}

$readme = @"
wkhtmltopdf-ng — standalone Windows CLI

Unpack this zip and run from the bin folder (or add bin to PATH):

  bin\wkhtmltopdf.exe input.html output.pdf
  bin\wkhtmltoimage.exe input.html output.png

No Qt install is required. windeployqt copied the WebEngine runtime
into this tree. Keep the folder together; do not copy only the .exe.

qpdf.exe is included when the packager could download it. Page-number
headers ([page]/[topage]), outlines, and --copies need qpdf.exe next
to the converter.

Headless use is supported. If a convert fails on a server, set:

  set QT_QPA_PLATFORM=offscreen
  set QTWEBENGINE_CHROMIUM_FLAGS=--no-sandbox --disable-gpu
"@
Set-Content -Path (Join-Path $stageDir "README.txt") -Value $readme

if (-not (Test-Path $Output)) { New-Item -ItemType Directory -Force -Path $Output | Out-Null }
$version = (Get-Content (Join-Path $Source "VERSION")).Trim()
$zip = Join-Path $Output "wkhtmltopdf-ng_${version}_windows-amd64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $zip -Force

Write-Host "created $zip"
