#ifndef WKHTMLTOPDF_NG_TOC_GENERATOR_H
#define WKHTMLTOPDF_NG_TOC_GENERATOR_H

#include "Settings.h"
#include "PdfPostProcessor.h"

#include <QList>
#include <QString>

QString tocJavascript(const ObjectSettings& settings);
QString tocHtmlDocument(const ObjectSettings& settings, const QList<OutlineEntry>& headings);
QString defaultTocXsl();
QString outlineXmlDocument(const QList<OutlineEntry>& headings);
bool applyTocXsl(const QString& xslPath, const QString& outlineXml, QString* html, QString* error);

#endif
