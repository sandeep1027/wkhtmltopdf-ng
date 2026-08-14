#ifndef WKHTMLTOPDF_NG_FILE_UTILS_H
#define WKHTMLTOPDF_NG_FILE_UTILS_H

#include <QByteArray>
#include <QString>

QByteArray readAllFromStdin();
QByteArray readAllFile(const QString& path, bool* ok = nullptr);
QString decodeText(const QByteArray& data, const QString& encoding);
bool writeAllFile(const QString& path, const QByteArray& data, QString* error = nullptr);
bool writeAllToStdout(const QByteArray& data, QString* error = nullptr);

#endif
