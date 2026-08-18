#include "TocGenerator.h"

#include "utils/JsBridge.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QXmlStreamWriter>

QString tocJavascript(const ObjectSettings& settings)
{
    const QString caption = javascriptString(settings.tocCaptionText);
    const QString header = javascriptString(settings.tocHeaderText);
    const QString indentation = javascriptString(settings.tocLevelIndentation);
    const QString shrink = QString::number(settings.tocTextSizeShrink, 'g', 12);
    const QString dotted = settings.tocUseDottedLines ? QStringLiteral("true") : QStringLiteral("false");
    const QString forwardLinks = settings.tocForwardLinks ? QStringLiteral("true") : QStringLiteral("false");
    const QString backLinks = settings.tocBackLinks ? QStringLiteral("true") : QStringLiteral("false");

    return QStringLiteral(R"JS(
(function() {
    const headings = Array.from(document.querySelectorAll('h1,h2,h3,h4,h5,h6'));
    const nav = document.createElement('section');
    nav.className = 'wkhtmltopdf-ng-toc';
    nav.setAttribute('role', 'doc-toc');
    nav.style.breakAfter = 'page';
    nav.style.pageBreakAfter = 'always';

    const style = document.createElement('style');
    style.textContent = `
      .wkhtmltopdf-ng-toc { font-size: ${SHRINK}em; page-break-after: always; break-after: page; }
      .wkhtmltopdf-ng-toc h1 { margin: 0 0 1em; }
      .wkhtmltopdf-ng-toc ol { list-style: none; margin: 0; padding: 0; }
      .wkhtmltopdf-ng-toc li { display: flex; align-items: baseline; margin: .25em 0; }
      .wkhtmltopdf-ng-toc a { color: inherit; text-decoration: none; }
      .wkhtmltopdf-ng-toc .wkhtmltopdf-ng-dots { flex: 1; border-bottom: 1px dotted currentColor; margin: 0 .5em .2em; }
    `;
    document.head.appendChild(style);

    const title = document.createElement('h1');
    title.textContent = CAPTION;
    nav.appendChild(title);
    const list = document.createElement('ol');
    nav.appendChild(list);

    headings.forEach((heading, index) => {
        const id = heading.id || ('wkhtmltopdf-ng-heading-' + index);
        heading.id = id;
        const item = document.createElement('li');
        const link = document.createElement('a');
        link.href = '#' + id;
        link.textContent = heading.textContent.trim();
        const level = Number(heading.tagName.substring(1));
        const indentValue = parseFloat(INDENTATION) || 1;
        const indentUnit = /[a-z%]+$/i.test(INDENTATION) ? INDENTATION.match(/[a-z%]+$/i)[0] : 'em';
        item.style.marginLeft = ((level - 1) * indentValue) + indentUnit;
        item.appendChild(link);
        if (DOTTED) {
            const dots = document.createElement('span');
            dots.className = 'wkhtmltopdf-ng-dots';
            dots.setAttribute('aria-hidden', 'true');
            item.appendChild(dots);
        }
        list.appendChild(item);
    });

    if (!FORWARD_LINKS) {
        list.querySelectorAll('a').forEach(link => link.removeAttribute('href'));
    }
    if (headings.length === 0) {
        const empty = document.createElement('p');
        empty.textContent = HEADER;
        list.appendChild(empty);
    }
    nav.id = 'wkhtmltopdf-ng-toc';
    if (BACK_LINKS) {
        headings.forEach(function(heading) {
            const link = document.createElement('a');
            link.href = '#wkhtmltopdf-ng-toc';
            while (heading.firstChild) link.appendChild(heading.firstChild);
            heading.appendChild(link);
        });
    }
    document.body.insertBefore(nav, document.body.firstChild);
})();
)JS")
        .replace(QStringLiteral("CAPTION"), caption)
        .replace(QStringLiteral("HEADER"), header)
        .replace(QStringLiteral("INDENTATION"), indentation)
        .replace(QStringLiteral("SHRINK"), shrink)
        .replace(QStringLiteral("DOTTED"), dotted)
        .replace(QStringLiteral("FORWARD_LINKS"), forwardLinks)
        .replace(QStringLiteral("BACK_LINKS"), backLinks);
}

QString tocHtmlDocument(const ObjectSettings& settings, const QList<OutlineEntry>& headings)
{
    const QString caption = settings.tocCaptionText.toHtmlEscaped();
    const QString empty = settings.tocHeaderText.toHtmlEscaped();
    const QString indent = settings.tocLevelIndentation;
    bool indentOk = false;
    const double indentValue = indent.left(indent.indexOf(QRegularExpression(QStringLiteral("[a-z%]+"),
        QRegularExpression::CaseInsensitiveOption))).toDouble(&indentOk);
    const double step = indentOk && indentValue > 0 ? indentValue : 1.0;
    QString indentUnit = QStringLiteral("em");
    const QRegularExpression unitExpression(QStringLiteral("([a-z%]+)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    const auto unitMatch = unitExpression.match(indent);
    if (unitMatch.hasMatch()) indentUnit = unitMatch.captured(1);

    QString items;
    int index = 0;
    for (const OutlineEntry& heading : headings) {
        const int level = qMax(1, heading.level);
        const QString href = settings.tocForwardLinks
            ? QStringLiteral("#wkhtmltopdf-ng-heading-%1").arg(index)
            : QString();
        items += QStringLiteral("<li style=\"margin-left:%1%2\">").arg((level - 1) * step).arg(indentUnit);
        if (!href.isEmpty()) {
            items += QStringLiteral("<a href=\"%1\">%2</a>")
                .arg(href, heading.title.toHtmlEscaped());
        } else {
            items += QStringLiteral("<span>%1</span>").arg(heading.title.toHtmlEscaped());
        }
        if (settings.tocUseDottedLines) {
            items += QStringLiteral("<span class=\"wkhtmltopdf-ng-dots\" aria-hidden=\"true\"></span>");
        }
        if (settings.tocPageNumbers && heading.page > 0) {
            items += QStringLiteral("<span class=\"wkhtmltopdf-ng-toc-page\">%1</span>")
                .arg(heading.page);
        }
        items += QStringLiteral("</li>\n");
        ++index;
    }
    if (items.isEmpty()) {
        items = QStringLiteral("<p>%1</p>").arg(empty);
    }

    return QStringLiteral(
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>%1</title>"
        "<style>"
        "body{font-family:sans-serif;font-size:%2em;}"
        "h1{margin:0 0 1em;}"
        "ol{list-style:none;margin:0;padding:0;}"
        "li{display:flex;align-items:baseline;margin:.25em 0;}"
        "a{color:inherit;text-decoration:none;}"
        ".wkhtmltopdf-ng-dots{flex:1;border-bottom:1px dotted currentColor;margin:0 .5em .2em;}"
        ".wkhtmltopdf-ng-toc-page{margin-left:auto;padding-left:.5em;}"
        "</style></head><body>"
        "<section class=\"wkhtmltopdf-ng-toc\" role=\"doc-toc\"><h1>%1</h1><ol>%3</ol></section>"
        "</body></html>")
        .arg(caption)
        .arg(settings.tocTextSizeShrink > 0 ? settings.tocTextSizeShrink : 0.8)
        .arg(items);
}

QString outlineXmlDocument(const QList<OutlineEntry>& headings)
{
    QString xml;
    QXmlStreamWriter writer(&xml);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("outline"));
    QList<int> levels;
    for (const OutlineEntry& heading : headings) {
        const int level = qMax(1, heading.level);
        while (!levels.isEmpty() && levels.last() >= level) {
            writer.writeEndElement();
            levels.removeLast();
        }
        writer.writeStartElement(QStringLiteral("item"));
        writer.writeAttribute(QStringLiteral("title"), heading.title);
        writer.writeAttribute(QStringLiteral("level"), QString::number(level));
        writer.writeAttribute(QStringLiteral("page"), QString::number(heading.page));
        if (!heading.link.isEmpty()) writer.writeAttribute(QStringLiteral("link"), heading.link);
        if (!heading.backLink.isEmpty())
            writer.writeAttribute(QStringLiteral("backLink"), heading.backLink);
        levels.append(level);
    }
    while (!levels.isEmpty()) {
        writer.writeEndElement();
        levels.removeLast();
    }
    writer.writeEndElement();
    writer.writeEndDocument();
    return xml;
}

QString defaultTocXsl()
{
    return QStringLiteral(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">\n"
        "  <xsl:output method=\"html\" encoding=\"UTF-8\" indent=\"yes\"/>\n"
        "  <xsl:template match=\"/\">\n"
        "    <html><head><meta charset=\"utf-8\"/><title>Table of Contents</title>\n"
        "    <style>body{font-family:sans-serif}ol{list-style:none;padding-left:1em}"
        "li{display:flex;align-items:baseline;margin:.25em 0}a{color:inherit;text-decoration:none}"
        ".wkhtmltopdf-ng-toc-page{margin-left:auto;padding-left:.5em}</style></head><body>\n"
        "    <h1>Table of Contents</h1>\n"
        "    <ol><xsl:apply-templates select=\"outline/item\"/></ol>\n"
        "    </body></html>\n"
        "  </xsl:template>\n"
        "  <xsl:template match=\"item\">\n"
        "    <li>\n"
        "      <xsl:choose>\n"
        "        <xsl:when test=\"@link\"><a href=\"{@link}\"><xsl:value-of select=\"@title\"/></a></xsl:when>\n"
        "        <xsl:otherwise><xsl:value-of select=\"@title\"/></xsl:otherwise>\n"
        "      </xsl:choose>\n"
        "      <xsl:if test=\"@page\"><span class=\"wkhtmltopdf-ng-toc-page\"><xsl:value-of select=\"@page\"/></span></xsl:if>\n"
        "      <xsl:if test=\"item\"><ol><xsl:apply-templates select=\"item\"/></ol></xsl:if>\n"
        "    </li>\n"
        "  </xsl:template>\n"
        "</xsl:stylesheet>\n");
}

bool applyTocXsl(const QString& xslPath, const QString& outlineXml, QString* html, QString* error)
{
    QTemporaryFile xmlFile(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-outline-XXXXXX.xml"));
    xmlFile.setAutoRemove(true);
    if (!xmlFile.open()) {
        if (error) *error = xmlFile.errorString();
        return false;
    }
    xmlFile.write(outlineXml.toUtf8());
    xmlFile.close();

    QString stylesheet = xslPath;
    QTemporaryFile defaultFile(QDir::tempPath() + QStringLiteral("/wkhtmltopdf-ng-toc-XXXXXX.xsl"));
    defaultFile.setAutoRemove(true);
    if (stylesheet.isEmpty() || stylesheet == QStringLiteral("-")) {
        if (!defaultFile.open()) {
            if (error) *error = defaultFile.errorString();
            return false;
        }
        defaultFile.write(defaultTocXsl().toUtf8());
        defaultFile.close();
        stylesheet = defaultFile.fileName();
    }

    QProcess process;
    process.start(QStringLiteral("xsltproc"), {stylesheet, xmlFile.fileName()});
    if (!process.waitForFinished(60000) || process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0) {
        if (error) {
            const QString output = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
            *error = output.isEmpty()
                ? QStringLiteral("xsltproc is required for --toc-xsl")
                : output;
        }
        return false;
    }
    if (html) *html = QString::fromUtf8(process.readAllStandardOutput());
    return html && !html->isEmpty();
}
