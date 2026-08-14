#ifndef WKHTMLTOPDF_NG_JS_BRIDGE_H
#define WKHTMLTOPDF_NG_JS_BRIDGE_H

#include <QString>

QString javascriptString(const QString& value);
QString javascriptStringFromBytes(const QByteArray& value);
QString readJavascriptFile(const QString& path, bool* ok = nullptr);
QString imageDownsampleJavascript(int dpi, int quality);

#endif
