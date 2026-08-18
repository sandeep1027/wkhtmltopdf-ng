#include "HtmlToImageConverter.h"

#include "utils/FileUtils.h"

#include <QBuffer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTimer>
#include <QUrl>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineView>

#include <functional>

namespace {

QUrl inputUrl(const QString& input)
{
    if (input.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive) ||
        input.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) ||
        input.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
        return QUrl(input);
    }
    return QUrl::fromLocalFile(QFileInfo(input).absoluteFilePath());
}

QString outputFormat(const QString& fmt, const QString& outputPath)
{
    QString suffix = fmt.trimmed().toLower();
    if (suffix.isEmpty() && outputPath != QStringLiteral("-")) {
        suffix = QFileInfo(outputPath).suffix().toLower();
    }
    if (suffix == QStringLiteral("jpg") || suffix == QStringLiteral("jpeg")) {
        return QStringLiteral("JPEG");
    }
    if (suffix == QStringLiteral("webp")) return QStringLiteral("WEBP");
    return QStringLiteral("PNG");
}

}

HtmlToImageConverter::HtmlToImageConverter(const ImageSettings& settings)
    : m_settings(settings)
{
}

QByteArray HtmlToImageConverter::convertToBuffer(const QString& input, QString* error)
{
    QWebEngineView view;
    int width = m_settings.screenWidth > 0 ? m_settings.screenWidth : 1024;
    view.resize(width, qMax(32, m_settings.screenHeight));
    view.show();
    if (m_settings.transparent) {
        view.setAttribute(Qt::WA_TranslucentBackground, true);
        view.page()->setBackgroundColor(Qt::transparent);
    }
    const double dpiScale = m_settings.dpi > 0 ? (m_settings.dpi / 96.0) : 1.0;
    const double zoom = (m_settings.zoom > 0.0 ? m_settings.zoom : 1.0) * dpiScale;
    if (zoom > 0.0) view.setZoomFactor(zoom);
    view.settings()->setAttribute(QWebEngineSettings::JavascriptEnabled, m_settings.enableJavascript);
    view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls,
                                  m_settings.enableLocalFileAccess);
    view.settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);

    QEventLoop loop;
    QImage result;
    QString failure;
    bool failed = false;

    auto fail = [&](const QString& message) {
        failed = true;
        failure = message;
        loop.quit();
    };

    std::function<void()> render;
    std::function<void()> afterDelay;
    render = [&]() {
        view.page()->runJavaScript(
            QStringLiteral("(function(){var b=document.body,d=document.documentElement;"
                           "return {w:Math.max(d.scrollWidth,b?b.scrollWidth:0),"
                           "h:Math.max(d.scrollHeight,b?b.scrollHeight:0)};})()"),
            [&](const QVariant& value) {
                const QVariantMap dimensions = value.toMap();
                const int contentWidth = dimensions.value(QStringLiteral("w")).toInt();
                const int contentHeight = dimensions.value(QStringLiteral("h")).toInt();
                if (m_settings.smartWidth && contentWidth > 0) width = qMax(width, contentWidth);
                if (m_settings.screenHeight <= 0 && contentHeight > 0) {
                    view.resize(width, qMax(width, contentHeight));
                }
                QTimer::singleShot(50, &view, [&]() {
                    QImage image = view.grab().toImage();
                    if (image.isNull()) {
                        fail(QStringLiteral("could not capture page"));
                        return;
                    }
                    if (m_settings.cropWidth > 0 || m_settings.cropHeight > 0) {
                        const int effectiveWidth = m_settings.cropWidth > 0
                            ? m_settings.cropWidth : image.width() - m_settings.cropLeft;
                        const int effectiveHeight = m_settings.cropHeight > 0
                            ? m_settings.cropHeight : image.height() - m_settings.cropTop;
                        const QRect crop(m_settings.cropLeft, m_settings.cropTop,
                                         effectiveWidth, effectiveHeight);
                        if (!QRect(QPoint(0, 0), image.size()).contains(crop.topLeft()) ||
                            !QRect(QPoint(0, 0), image.size()).contains(crop.bottomRight())) {
                            fail(QStringLiteral("crop rectangle is outside the captured image"));
                            return;
                        }
                        image = image.copy(crop);
                    }
                    result = image;
                    loop.quit();
                });
            });
    };

    afterDelay = [&]() {
        if (!m_settings.enableJavascript) {
            render();
            return;
        }
        view.page()->runJavaScript(
            QStringLiteral("(function(){const fonts=!document.fonts||document.fonts.status!=='loading';"
                           "const images=Array.from(document.images).every(function(img){return img.complete;});"
                           "return fonts&&images;})()"),
            [&](const QVariant& value) {
                if (value.toBool()) render();
                else QTimer::singleShot(50, &view, afterDelay);
            });
    };

    QObject::connect(&view, &QWebEngineView::loadFinished, &view, [&](bool ok) {
        if (!ok) {
            fail(QStringLiteral("failed to load %1").arg(input));
            return;
        }
        if (m_settings.javascriptDelay > 0) {
            QTimer::singleShot(m_settings.javascriptDelay, &view, afterDelay);
        } else {
            afterDelay();
        }
    });

    view.load(inputUrl(input));
    loop.exec();
    if (failed) {
        if (error) *error = failure;
        return {};
    }

    const QString format = outputFormat(m_settings.fmt, m_settings.out);
    if (m_settings.transparent && format == QStringLiteral("JPEG")) {
        if (error) *error = QStringLiteral("transparent output requires PNG or WebP format");
        return {};
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    const int quality = qBound(0, m_settings.quality, 100);
    if (!buffer.open(QIODevice::WriteOnly) ||
        !result.save(&buffer, format.toLatin1().constData(), quality)) {
        if (error) *error = QStringLiteral("could not encode image");
        return {};
    }
    return bytes;
}

bool HtmlToImageConverter::convert(const QString& input, const QString& outputPath, QString* error)
{
    const QByteArray bytes = convertToBuffer(input, error);
    if (bytes.isEmpty()) return false;
    if (outputPath == QStringLiteral("-")) {
        if (!writeAllToStdout(bytes, error)) return false;
    } else if (!writeAllFile(outputPath, bytes, error)) {
        return false;
    }
    return true;
}
