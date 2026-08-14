#ifndef WKHTMLTOPDF_NG_PAGE_LAYOUT_H
#define WKHTMLTOPDF_NG_PAGE_LAYOUT_H

#include <QPageLayout>
#include <QPageRanges>
#include <QString>

struct PageLayoutOptions {
    QString pageSize = QStringLiteral("A4");
    QPageLayout::Orientation orientation = QPageLayout::Portrait;
    double marginTop = 10.0;
    double marginRight = 10.0;
    double marginBottom = 10.0;
    double marginLeft = 10.0;
};

QPageLayout makePageLayout(const PageLayoutOptions& options);
bool parseLengthMm(const QString& value, double* millimeters);
bool parsePageSize(const QString& value, QPageSize* pageSize);
bool parsePageRanges(const QString& value, QPageRanges* ranges);

#endif
