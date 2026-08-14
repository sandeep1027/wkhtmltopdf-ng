#include "PageLayout.h"

#include <QRegularExpression>
#include <QList>

namespace {

QPageSize::PageSizeId pageSizeId(const QString& value)
{
    const QString name = value.trimmed().toLower();
    const QList<QPair<QString, QPageSize::PageSizeId>> sizes = {
        {QStringLiteral("a0"), QPageSize::A0}, {QStringLiteral("a1"), QPageSize::A1},
        {QStringLiteral("a2"), QPageSize::A2}, {QStringLiteral("a3"), QPageSize::A3},
        {QStringLiteral("a4"), QPageSize::A4}, {QStringLiteral("a5"), QPageSize::A5},
        {QStringLiteral("a6"), QPageSize::A6}, {QStringLiteral("a7"), QPageSize::A7},
        {QStringLiteral("a8"), QPageSize::A8}, {QStringLiteral("a9"), QPageSize::A9},
        {QStringLiteral("b0"), QPageSize::B0}, {QStringLiteral("b1"), QPageSize::B1},
        {QStringLiteral("b2"), QPageSize::B2}, {QStringLiteral("b3"), QPageSize::B3},
        {QStringLiteral("b4"), QPageSize::B4}, {QStringLiteral("b5"), QPageSize::B5},
        {QStringLiteral("b6"), QPageSize::B6}, {QStringLiteral("b7"), QPageSize::B7},
        {QStringLiteral("b8"), QPageSize::B8}, {QStringLiteral("b9"), QPageSize::B9},
        {QStringLiteral("b10"), QPageSize::B10}, {QStringLiteral("letter"), QPageSize::Letter},
        {QStringLiteral("legal"), QPageSize::Legal}, {QStringLiteral("executive"), QPageSize::Executive},
        {QStringLiteral("tabloid"), QPageSize::Tabloid}, {QStringLiteral("ledger"), QPageSize::Ledger},
        {QStringLiteral("folio"), QPageSize::Folio}, {QStringLiteral("c5e"), QPageSize::C5E},
        {QStringLiteral("comm10e"), QPageSize::Comm10E}, {QStringLiteral("dle"), QPageSize::DLE}
    };
    for (const auto& pair : sizes) {
        if (pair.first == name) return pair.second;
    }
    return QPageSize::Custom;
}

}

bool parseLengthMm(const QString& value, double* millimeters)
{
    if (!millimeters) {
        return false;
    }

    const QRegularExpression expression(
        QStringLiteral(R"(^\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+))\s*(mm|cm|in|pt|px)?\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = expression.match(value);
    if (!match.hasMatch()) {
        return false;
    }

    const double number = match.captured(1).toDouble();
    const QString unit = match.captured(2).toLower();
    double factor = 1.0;
    if (unit == QStringLiteral("cm")) {
        factor = 10.0;
    } else if (unit == QStringLiteral("in")) {
        factor = 25.4;
    } else if (unit == QStringLiteral("pt")) {
        factor = 25.4 / 72.0;
    } else if (unit == QStringLiteral("px")) {
        factor = 25.4 / 96.0;
    }

    *millimeters = number * factor;
    return true;
}

bool parsePageSize(const QString& value, QPageSize* pageSize)
{
    if (!pageSize) {
        return false;
    }

    const QString normalized = value.trimmed();
    const QPageSize::PageSizeId id = pageSizeId(normalized);
    if (id == QPageSize::Custom) {
        return false;
    }

    *pageSize = QPageSize(id);
    return true;
}

bool parsePageRanges(const QString& value, QPageRanges* ranges)
{
    if (!ranges) return false;
    const QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        *ranges = QPageRanges();
        return true;
    }
    const QPageRanges parsed = QPageRanges::fromString(normalized);
    if (parsed.isEmpty()) return false;
    *ranges = parsed;
    return true;
}

QPageLayout makePageLayout(const PageLayoutOptions& options)
{
    QPageSize pageSize;
    if (!parsePageSize(options.pageSize, &pageSize)) {
        pageSize = QPageSize(QPageSize::A4);
    }

    return QPageLayout(
        pageSize,
        options.orientation,
        QMarginsF(options.marginLeft, options.marginTop,
                  options.marginRight, options.marginBottom),
        QPageLayout::Millimeter);
}
