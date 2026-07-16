#include "fb2contentrenderer.h"

#include "richtextprocessor.h"
#include "xmlutils.h"

#include <QDomDocument>
#include <QHash>
#include <QTextDocument>

#include <memory>

namespace {

using DocumentXml::attributeByName;
using DocumentXml::directChildElementByName;
using DocumentXml::elementName;
using DocumentXml::elementsByName;
using DocumentXml::nodeText;
using DocumentXml::normalizedBlock;

constexpr qsizetype maximumFb2ImageSize = 16 * 1024 * 1024;

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString anchorId(const QDomElement &element)
{
    return attributeByName(element, QStringLiteral("id")).trimmed();
}

QString anchoredInline(const QString &id, const QString &content)
{
    return id.isEmpty()
               ? content
               : QStringLiteral("<a name=\"%1\">%2</a>")
                     .arg(escaped(id), content);
}

struct RenderContext final
{
    QHash<QString, EmbeddedDocumentResource> images;
};

QString renderChildren(const QDomNode &root,
                       const RenderContext &context,
                       int sectionDepth,
                       const QString &leadingAnchor = {});

QString renderElement(const QDomElement &element,
                      const RenderContext &context,
                      int sectionDepth,
                      const QString &inheritedAnchor = {})
{
    const QString name = elementName(element);
    const QString ownAnchor = anchorId(element);
    const QString anchor = ownAnchor.isEmpty() ? inheritedAnchor : ownAnchor;

    if (name == QLatin1String("binary") || name == QLatin1String("description")) {
        return {};
    }
    if (name == QLatin1String("section")) {
        return QStringLiteral("<section>%1</section>")
            .arg(renderChildren(element,
                                context,
                                sectionDepth + 1,
                                anchor));
    }
    if (name == QLatin1String("title")) {
        const int level = qBound(1, sectionDepth, 6);
        const QString title = normalizedBlock(nodeText(element));
        return title.isEmpty()
                   ? QString()
                   : QStringLiteral("<h%1>%2</h%1>")
                         .arg(level)
                         .arg(anchoredInline(anchor, escaped(title)));
    }
    if (name == QLatin1String("subtitle")) {
        return QStringLiteral("<h3>%1</h3>")
            .arg(anchoredInline(
                anchor,
                escaped(normalizedBlock(nodeText(element)))));
    }
    if (name == QLatin1String("p") || name == QLatin1String("v")) {
        return QStringLiteral("<p>%1</p>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("empty-line")) {
        return QStringLiteral("<p>%1</p>")
            .arg(anchoredInline(anchor, QStringLiteral("<br>")));
    }
    if (name == QLatin1String("emphasis")) {
        return QStringLiteral("<em>%1</em>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("strong")) {
        return QStringLiteral("<strong>%1</strong>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("strikethrough")) {
        return QStringLiteral("<s>%1</s>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("code")) {
        return QStringLiteral("<code>%1</code>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("a")) {
        QString href = attributeByName(element, QStringLiteral("href")).trimmed();
        if (!href.isEmpty() && !href.startsWith(u'#') && !href.contains(u':')) {
            href.prepend(u'#');
        }
        const bool noteLink = attributeByName(element, QStringLiteral("type"))
                                  .compare(QStringLiteral("note"),
                                           Qt::CaseInsensitive) == 0;
        const QString linkText = renderChildren(element, context, sectionDepth);
        const QString nameAttribute = anchor.isEmpty()
                                          ? QString()
                                          : QStringLiteral(" name=\"%1\"")
                                                .arg(escaped(anchor));
        return QStringLiteral("<a%1 href=\"%2\">%3%4%5</a>")
            .arg(nameAttribute,
                 escaped(href),
                 noteLink ? QStringLiteral("<sup>") : QString(),
                 linkText,
                 noteLink ? QStringLiteral("</sup>") : QString());
    }
    if (name == QLatin1String("image")) {
        QString imageId = attributeByName(element, QStringLiteral("href")).trimmed();
        if (imageId.startsWith(u'#')) {
            imageId.remove(0, 1);
        }
        const EmbeddedDocumentResource resource = context.images.value(imageId);
        const QString source = RichTextProcessor::embeddedDataUrl(resource);
        return source.isEmpty()
                   ? QString()
                   : anchoredInline(
                         anchor,
                         QStringLiteral("<img src=\"%1\" alt=\"%2\">")
                             .arg(source,
                                  escaped(attributeByName(
                                      element, QStringLiteral("alt")))));
    }
    if (name == QLatin1String("epigraph")
        || name == QLatin1String("cite")
        || name == QLatin1String("annotation")) {
        return QStringLiteral("<blockquote>%1</blockquote>")
            .arg(renderChildren(element, context, sectionDepth, anchor));
    }
    if (name == QLatin1String("poem") || name == QLatin1String("stanza")) {
        return QStringLiteral("<div>%1</div>")
            .arg(renderChildren(element, context, sectionDepth, anchor));
    }
    if (name == QLatin1String("text-author") || name == QLatin1String("date")) {
        return QStringLiteral("<p><em>%1</em></p>")
            .arg(anchoredInline(
                anchor,
                renderChildren(element, context, sectionDepth)));
    }
    if (name == QLatin1String("table")) {
        return QStringLiteral("<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\">%1</table>")
            .arg(renderChildren(element, context, sectionDepth, anchor));
    }
    if (name == QLatin1String("tr")) {
        return QStringLiteral("<tr>%1</tr>")
            .arg(renderChildren(element, context, sectionDepth, anchor));
    }
    if (name == QLatin1String("td") || name == QLatin1String("th")) {
        return QStringLiteral("<%1>%2</%1>")
            .arg(name,
                 renderChildren(element, context, sectionDepth, anchor));
    }

    return anchoredInline(anchor,
                          renderChildren(element, context, sectionDepth));
}

QString renderChildren(const QDomNode &root,
                       const RenderContext &context,
                       int sectionDepth,
                       const QString &leadingAnchor)
{
    QString html;
    bool anchorPending = !leadingAnchor.isEmpty();
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        if (node.isText() || node.isCDATASection()) {
            const QString text = escaped(node.nodeValue());
            if (anchorPending && !node.nodeValue().trimmed().isEmpty()) {
                html += anchoredInline(leadingAnchor, text);
                anchorPending = false;
            } else {
                html += text;
            }
            continue;
        }

        const QDomElement element = node.toElement();
        if (!element.isNull()) {
            const QString rendered = renderElement(
                element,
                context,
                sectionDepth,
                anchorPending ? leadingAnchor : QString());
            if (!rendered.isEmpty()) {
                html += rendered;
                anchorPending = false;
            }
        }
    }
    return html;
}

RenderContext renderContext(const QDomDocument &document)
{
    RenderContext context;
    for (const QDomElement &binary : elementsByName(document, QStringLiteral("binary"))) {
        const QString id = attributeByName(binary, QStringLiteral("id")).trimmed();
        const QString mimeType = attributeByName(
            binary, QStringLiteral("content-type")).trimmed();
        const QByteArray data = QByteArray::fromBase64(binary.text().toLatin1());
        if (!id.isEmpty()
            && data.size() <= maximumFb2ImageSize
            && mimeType.startsWith(QStringLiteral("image/"))) {
            context.images.insert(id, {data, mimeType});
        }
    }
    return context;
}

} // namespace

Fb2RenderedContent Fb2ContentRenderer::render(const QDomDocument &document)
{
    const RenderContext context = renderContext(document);
    QStringList chapterTitles;
    QString bodyHtml;

    for (const QDomElement &body : elementsByName(document, QStringLiteral("body"))) {
        const bool notesBody = attributeByName(body, QStringLiteral("name"))
                                   .compare(QStringLiteral("notes"),
                                            Qt::CaseInsensitive) == 0;
        if (!notesBody) {
            for (const QDomElement &section
                 : elementsByName(body, QStringLiteral("section"))) {
                const QString title = normalizedBlock(nodeText(
                    directChildElementByName(section, QStringLiteral("title"))));
                if (!title.isEmpty()) {
                    chapterTitles.append(title);
                }
            }
        } else if (!bodyHtml.isEmpty()) {
            bodyHtml += QStringLiteral("<hr><h1>Notes</h1>");
        }
        bodyHtml += renderChildren(body, context, 1);
    }

    const QString html = QStringLiteral(
        "<html><head><style>"
        "h1,h2,h3{font-weight:600;}"
        "blockquote{margin-left:24px;}"
        "table{border-collapse:collapse;}"
        "td,th{border:1px solid #888;}"
        "</style></head><body>%1</body></html>")
                             .arg(bodyHtml);
    const std::unique_ptr<QTextDocument> richDocument = RichTextProcessor::fromHtml(html);
    return {RichTextProcessor::content(*richDocument), chapterTitles};
}
