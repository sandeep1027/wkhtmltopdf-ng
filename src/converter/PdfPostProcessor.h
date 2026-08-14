#ifndef WKHTMLTOPDF_NG_PDF_POST_PROCESSOR_H
#define WKHTMLTOPDF_NG_PDF_POST_PROCESSOR_H

#include "Settings.h"

#include <QString>
#include <QList>

struct OutlineEntry {
    QString title;
    int level = 1;
    int page = 1;
    QString link;
    QString backLink;
};

struct PlaceholderContext {
    int page = 1;
    int pages = 1;
    int pageOffset = 0;
    int sitePage = 1;
    int sitePages = 1;
    QString title;
    QString documentTitle;
    QString webpage;
    QString section;
    QString subsection;
};

QString replaceHeaderTokens(QString value, const PlaceholderContext& context);

bool hasPageNumberPlaceholders(const ObjectSettings& settings);
bool applyPageNumberOverlay(const QString& inputPath, const QString& outputPath,
                            const GlobalSettings& global, const ObjectSettings& settings,
                            QString* error = nullptr);
bool embedPdfOutlines(const QString& inputPath, const QString& outputPath,
                      const QList<OutlineEntry>& entries, QString* error = nullptr);
bool applyPdfCopiesAndCompression(const QString& inputPath, const QString& outputPath,
                                  const GlobalSettings& global, QString* error = nullptr);
int countPdfPages(const QString& path);
bool mergePdfFiles(const QStringList& inputPaths, const QString& outputPath, QString* error = nullptr);
bool applyPdfPageRanges(const QString& inputPath, const QString& outputPath,
                        const QString& ranges, QString* error = nullptr);
bool splitPdfPages(const QString& inputPath, const QString& outputPath, QString* error = nullptr);
bool insertPdfAfterPage(const QString& originalPath, const QString& insertPath,
                        int afterPage, const QString& outputPath, QString* error = nullptr);
bool runPdfEdit(const GlobalSettings& global, const QStringList& inputs,
                const QString& output, QString* error = nullptr);
bool applyPdfDocumentExtras(const QString& inputPath, const QString& outputPath,
                            const GlobalSettings& global, QString* error = nullptr);
bool pdfNeedsDocumentExtras(const GlobalSettings& global);
void remapOutlinePages(QList<OutlineEntry>* entries, int actualPages);
bool pdfNeedsImageOptimization(const GlobalSettings& global);
void effectiveImageOptimization(const GlobalSettings& global, int* dpi, int* quality);

#endif
