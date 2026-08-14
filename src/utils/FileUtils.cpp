#include "FileUtils.h"

#include <QFile>
#include <QStringConverter>
#include <QStringDecoder>

#include <cstdio>

QByteArray readAllFromStdin()
{
    QFile file;
    if (!file.open(stdin, QIODevice::ReadOnly)) return {};
    return file.readAll();
}

QByteArray readAllFile(const QString& path, bool* ok)
{
    QFile file(path);
    const bool opened = file.open(QIODevice::ReadOnly);
    if (ok) *ok = opened;
    return opened ? file.readAll() : QByteArray();
}

QString decodeText(const QByteArray& data, const QString& encoding)
{
    const QByteArray name = encoding.toUtf8();
    QStringDecoder decoder(name.constData());
    if (!decoder.isValid()) decoder = QStringDecoder(QStringDecoder::Utf8);
    return decoder.decode(data);
}

bool writeAllFile(const QString& path, const QByteArray& data, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) *error = file.errorString();
        return false;
    }
    if (file.write(data) != data.size()) {
        if (error) *error = file.errorString();
        return false;
    }
    return true;
}

bool writeAllToStdout(const QByteArray& data, QString* error)
{
    const qint64 written = std::fwrite(data.constData(), 1, static_cast<size_t>(data.size()), stdout);
    if (written != data.size()) {
        if (error) *error = QStringLiteral("failed to write PDF to stdout");
        return false;
    }
    if (std::fflush(stdout) != 0) {
        if (error) *error = QStringLiteral("failed to flush PDF stdout");
        return false;
    }
    return true;
}
