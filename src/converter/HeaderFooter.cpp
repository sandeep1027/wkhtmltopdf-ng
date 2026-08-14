#include "HeaderFooter.h"

#include "utils/JsBridge.h"

namespace {

QString contentFor(const QString& left, const QString& center, const QString& right)
{
    return QStringLiteral(
        "<div class=\"wkhtmltopdf-ng-left\">%1</div>"
        "<div class=\"wkhtmltopdf-ng-center\">%2</div>"
        "<div class=\"wkhtmltopdf-ng-right\">%3</div>")
        .arg(left.toHtmlEscaped(), center.toHtmlEscaped(), right.toHtmlEscaped());
}

}

QString headerFooterJavascript(const ObjectSettings& settings)
{
    QString headerFontName = settings.headerFontName;
    QString footerFontName = settings.footerFontName;
    const QString header = contentFor(settings.headerLeft, settings.headerCenter, settings.headerRight);
    const QString footer = contentFor(settings.footerLeft, settings.footerCenter, settings.footerRight);
    const QString headerHtml = settings.headerHtml.isEmpty() ? header : QString();
    const QString footerHtml = settings.footerHtml.isEmpty() ? footer : QString();
    const bool hasHeader = !settings.headerHtml.isEmpty() ||
        !settings.headerLeft.isEmpty() || !settings.headerCenter.isEmpty() || !settings.headerRight.isEmpty();
    const bool hasFooter = !settings.footerHtml.isEmpty() ||
        !settings.footerLeft.isEmpty() || !settings.footerCenter.isEmpty() || !settings.footerRight.isEmpty();

    if (!hasHeader && !hasFooter) return QString();

    const QString headerExpression = javascriptString(headerHtml);
    const QString footerExpression = javascriptString(footerHtml);
    const QString css = QStringLiteral(
        "@media print {"
        ".wkhtmltopdf-ng-header,.wkhtmltopdf-ng-footer{position:fixed;left:0;right:0;display:grid;"
        "grid-template-columns:1fr auto 1fr;align-items:center;z-index:2147483647;}"
        ".wkhtmltopdf-ng-header{top:0;padding-top:%1mm;font-family:'%2';font-size:%3pt;}"
        ".wkhtmltopdf-ng-footer{bottom:0;padding-bottom:%4mm;font-family:'%5';font-size:%6pt;}"
        ".wkhtmltopdf-ng-left{text-align:left}.wkhtmltopdf-ng-center{text-align:center}.wkhtmltopdf-ng-right{text-align:right}"
        ".wkhtmltopdf-ng-header hr,.wkhtmltopdf-ng-footer hr{border:0;border-top:1px solid #000;margin:2px 0 0;grid-column:1 / -1;}"
        "}").arg(settings.headerSpacing).arg(headerFontName.replace(QLatin1Char('\''), QStringLiteral("")))
        .arg(settings.headerFontSize).arg(settings.footerSpacing)
        .arg(footerFontName.replace(QLatin1Char('\''), QStringLiteral("")))
        .arg(settings.footerFontSize);

    QString script = QStringLiteral("(function(){\n"
        "const style=document.createElement('style');style.textContent=%1;document.head.appendChild(style);\n")
        .arg(javascriptString(css));

    if (hasHeader) {
        script += QStringLiteral("const h=document.createElement('div');h.className='wkhtmltopdf-ng-header';")
            + (settings.headerHtml.isEmpty()
                ? QStringLiteral("h.innerHTML=%1;").arg(headerExpression)
                : QStringLiteral("h.innerHTML=%1;").arg(javascriptString(settings.headerHtml)))
            + QStringLiteral("%1document.body.appendChild(h);\n")
                .arg(settings.headerLine ? QStringLiteral("h.insertAdjacentHTML('beforeend','<hr>');") : QString());
    }
    if (hasFooter) {
        script += QStringLiteral("const f=document.createElement('div');f.className='wkhtmltopdf-ng-footer';")
            + (settings.footerHtml.isEmpty()
                ? QStringLiteral("f.innerHTML=%1;").arg(footerExpression)
                : QStringLiteral("f.innerHTML=%1;").arg(javascriptString(settings.footerHtml)))
            + QStringLiteral("%1document.body.appendChild(f);\n")
                .arg(settings.footerLine ? QStringLiteral("f.insertAdjacentHTML('beforeend','<hr>');") : QString());
    }
    script += QStringLiteral(
        "document.querySelectorAll('.wkhtmltopdf-ng-header,.wkhtmltopdf-ng-footer').forEach(function(el){"
        "el.innerHTML=el.innerHTML.replace(/\\[title\\]/g,document.title||'')"
        ".replace(/\\[doctitle\\]/g,document.title||'')"
        ".replace(/\\[url\\]/g,location.href).replace(/\\[webpage\\]/g,location.href)"
        ".replace(/\\[frompage\\]/g,'1')"
        ".replace(/\\[section\\]/g,(document.querySelector('h1')||{}).textContent||'')"
        ".replace(/\\[subsection\\]/g,(document.querySelector('h2')||{}).textContent||'')"
        ".replace(/\\[isodate\\]/g,new Date().toISOString().slice(0,10))"
        ".replace(/\\[date\\]/g,new Date().toLocaleDateString())"
        ".replace(/\\[time\\]/g,new Date().toLocaleTimeString());});\n");
    for (auto it = settings.replacements.cbegin(); it != settings.replacements.cend(); ++it) {
        script += QStringLiteral(
            "document.querySelectorAll('.wkhtmltopdf-ng-header,.wkhtmltopdf-ng-footer').forEach(function(el){"
            "el.innerHTML=el.innerHTML.split(%1).join(%2);});\n")
            .arg(javascriptString(it.key()), javascriptString(it.value()));
    }
    script += QStringLiteral("})();");
    return script;
}
