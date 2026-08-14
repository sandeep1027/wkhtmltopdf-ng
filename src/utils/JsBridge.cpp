#include "JsBridge.h"

#include "FileUtils.h"

#include <QByteArray>
#include <QtGlobal>

QString javascriptStringFromBytes(const QByteArray& value)
{
    return QStringLiteral("new TextDecoder().decode(Uint8Array.from(atob('%1'), c => c.charCodeAt(0)))")
        .arg(QString::fromLatin1(value.toBase64()));
}

QString javascriptString(const QString& value)
{
    return javascriptStringFromBytes(value.toUtf8());
}

QString imageDownsampleJavascript(int dpi, int quality)
{
    const int safeDpi = qMax(1, dpi);
    const int safeQuality = qBound(0, quality, 100);
    return QStringLiteral(R"JS(
(function(maxDpi, jpegQuality){
  const q = Math.max(0, Math.min(1, jpegQuality / 100));
  function fit(naturalW, naturalH, displayW) {
    const inches = Math.max(displayW, 1) / 96;
    const maxW = Math.max(1, Math.round(inches * maxDpi));
    if (naturalW <= maxW) return null;
    return {w: maxW, h: Math.max(1, Math.round(naturalH * maxW / naturalW))};
  }
  function raster(source, w, h, png) {
    const canvas = document.createElement('canvas');
    canvas.width = w;
    canvas.height = h;
    canvas.getContext('2d').drawImage(source, 0, 0, w, h);
    try { return canvas.toDataURL(png ? 'image/png' : 'image/jpeg', q); }
    catch (e) { return null; }
  }
  const jobs = [];
  Array.from(document.images).forEach(function(img) {
    jobs.push(new Promise(function(resolve) {
      function go() {
        if (!img.naturalWidth) { resolve(); return; }
        const box = img.getBoundingClientRect();
        const size = fit(img.naturalWidth, img.naturalHeight, box.width || img.width || img.naturalWidth);
        if (!size) { resolve(); return; }
        const png = /image\/png|\.png(\?|$)/i.test(img.currentSrc || img.src || '');
        const url = raster(img, size.w, size.h, png);
        if (!url) { resolve(); return; }
        img.onload = img.onerror = function() { img.onload = img.onerror = null; resolve(); };
        img.removeAttribute('srcset');
        img.src = url;
      }
      if (img.complete) go();
      else { img.onload = go; img.onerror = function() { resolve(); }; }
    }));
  });
  Array.from(document.querySelectorAll('*')).forEach(function(el) {
    const bg = getComputedStyle(el).backgroundImage;
    const match = /url\(\s*['"]?([^'")]+)['"]?\s*\)/.exec(bg || '');
    if (!match || match[1].indexOf('data:') === 0) return;
    jobs.push(new Promise(function(resolve) {
      const probe = new Image();
      probe.onload = function() {
        const box = el.getBoundingClientRect();
        const size = fit(probe.naturalWidth, probe.naturalHeight, box.width || probe.naturalWidth);
        if (!size) { resolve(); return; }
        const url = raster(probe, size.w, size.h, false);
        if (url) el.style.backgroundImage = 'url(' + JSON.stringify(url) + ')';
        resolve();
      };
      probe.onerror = function() { resolve(); };
      probe.src = match[1];
    }));
  });
  window.__wkhtmltopdfNgImages = false;
  Promise.all(jobs).then(function() { window.__wkhtmltopdfNgImages = true; })
    .catch(function() { window.__wkhtmltopdfNgImages = true; });
  return true;
})(%1, %2)
)JS").arg(safeDpi).arg(safeQuality);
}

QString readJavascriptFile(const QString& path, bool* ok)
{
    bool readOk = false;
    const QByteArray data = readAllFile(path, &readOk);
    if (ok) *ok = readOk;
    return readOk ? QString::fromUtf8(data) : QString();
}
