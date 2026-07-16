#include "docxcontentrenderer.h"

#include "../archive/ziparchivereader.h"
#include "richtextprocessor.h"
#include "xmlutils.h"

#include <QDir>
#include <QDomDocument>
#include <QHash>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QTextDocument>

#include <memory>

namespace {

using DocumentXml::attributeByName;
using DocumentXml::directChildElementByName;
using DocumentXml::directChildElementsByName;
using DocumentXml::elementName;
using DocumentXml::elementsByName;
using DocumentXml::firstElementByName;
using DocumentXml::nodeText;
using DocumentXml::normalizedBlock;

constexpr qsizetype maximumDocxImageSize = 16 * 1024 * 1024;

struct Relationship final
{
    QString target;
    QString type;
    bool external = false;
};

struct StyleInfo final
{
    QString name;
    int headingLevel = 0;
};

struct NumberingInfo final
{
    QHash<QString, QString> abstractByNumber;
    QHash<QString, QHash<int, QString>> formatsByAbstract;
};

struct RenderContext final
{
    const ZipArchiveReader &zip;
    QHash<QString, Relationship> relationships;
    QHash<QString, StyleInfo> styles;
    NumberingInfo numbering;
};

struct ParagraphContent final
{
    QString innerHtml;
    QString plainText;
    QString alignmentStyle;
    QString listTag;
    int headingLevel = 0;
};

QString escaped(const QString &text)
{
    return text.toHtmlEscaped();
}

QString cleanArchivePath(QString path)
{
    path.replace(u'\\', u'/');
    path = QDir::cleanPath(path);
    while (path.startsWith(QStringLiteral("../"))) {
        path.remove(0, 3);
    }
    return path.startsWith(u'/') ? path.mid(1) : path;
}

QDomDocument archiveXml(const ZipArchiveReader &zip, const QString &path)
{
    QDomDocument document;
    const QByteArray bytes = zip.fileData(path);
    if (!bytes.isEmpty()) {
        document.setContent(bytes);
    }
    return document;
}

int headingLevelFromName(const QString &styleName)
{
    static const QRegularExpression headingPattern(
        QStringLiteral("(?:heading|title)[^0-9]*([1-6])?$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = headingPattern.match(styleName.trimmed());
    if (!match.hasMatch()) {
        return 0;
    }
    return match.captured(1).isEmpty() ? 1 : match.captured(1).toInt();
}

QHash<QString, Relationship> relationships(const ZipArchiveReader &zip)
{
    QHash<QString, Relationship> result;
    const QDomDocument document = archiveXml(
        zip, QStringLiteral("word/_rels/document.xml.rels"));
    for (const QDomElement &relationship
         : elementsByName(document, QStringLiteral("Relationship"))) {
        const QString id = attributeByName(relationship, QStringLiteral("Id"));
        if (!id.isEmpty()) {
            result.insert(id,
                          {attributeByName(relationship, QStringLiteral("Target")),
                           attributeByName(relationship, QStringLiteral("Type")),
                           attributeByName(relationship,
                                           QStringLiteral("TargetMode"))
                                   .compare(QStringLiteral("External"),
                                            Qt::CaseInsensitive) == 0});
        }
    }
    return result;
}

QHash<QString, StyleInfo> styles(const ZipArchiveReader &zip)
{
    QHash<QString, StyleInfo> result;
    const QDomDocument document = archiveXml(zip, QStringLiteral("word/styles.xml"));
    for (const QDomElement &style : elementsByName(document, QStringLiteral("style"))) {
        const QString id = attributeByName(style, QStringLiteral("styleId"));
        if (id.isEmpty()) {
            continue;
        }

        const QString name = attributeByName(
            directChildElementByName(style, QStringLiteral("name")),
            QStringLiteral("val"));
        int headingLevel = headingLevelFromName(id);
        if (headingLevel == 0) {
            headingLevel = headingLevelFromName(name);
        }
        const QString outlineLevel = attributeByName(
            firstElementByName(style, QStringLiteral("outlineLvl")),
            QStringLiteral("val"));
        if (!outlineLevel.isEmpty()) {
            headingLevel = qBound(1, outlineLevel.toInt() + 1, 6);
        }
        result.insert(id, {name, headingLevel});
    }
    return result;
}

NumberingInfo numbering(const ZipArchiveReader &zip)
{
    NumberingInfo result;
    const QDomDocument document = archiveXml(zip, QStringLiteral("word/numbering.xml"));
    for (const QDomElement &abstractNumber
         : elementsByName(document, QStringLiteral("abstractNum"))) {
        const QString abstractId = attributeByName(
            abstractNumber, QStringLiteral("abstractNumId"));
        QHash<int, QString> levelFormats;
        for (const QDomElement &level
             : directChildElementsByName(abstractNumber, QStringLiteral("lvl"))) {
            const int levelIndex = attributeByName(level, QStringLiteral("ilvl")).toInt();
            levelFormats.insert(
                levelIndex,
                attributeByName(directChildElementByName(
                                    level, QStringLiteral("numFmt")),
                                QStringLiteral("val")));
        }
        result.formatsByAbstract.insert(abstractId, levelFormats);
    }

    for (const QDomElement &number : elementsByName(document, QStringLiteral("num"))) {
        const QString numberId = attributeByName(number, QStringLiteral("numId"));
        const QString abstractId = attributeByName(
            directChildElementByName(number, QStringLiteral("abstractNumId")),
            QStringLiteral("val"));
        result.abstractByNumber.insert(numberId, abstractId);
    }
    return result;
}

QString listTag(const QDomElement &paragraphProperties,
                const NumberingInfo &numberingInfo)
{
    const QDomElement numberProperties = directChildElementByName(
        paragraphProperties, QStringLiteral("numPr"));
    if (numberProperties.isNull()) {
        return {};
    }

    const QString numberId = attributeByName(
        directChildElementByName(numberProperties, QStringLiteral("numId")),
        QStringLiteral("val"));
    const int level = attributeByName(
                          directChildElementByName(numberProperties,
                                                   QStringLiteral("ilvl")),
                          QStringLiteral("val"))
                          .toInt();
    const QString format = numberingInfo.formatsByAbstract
                               .value(numberingInfo.abstractByNumber.value(numberId))
                               .value(level);
    return format.compare(QStringLiteral("bullet"), Qt::CaseInsensitive) == 0
               ? QStringLiteral("ul")
               : QStringLiteral("ol");
}

QString imageHtml(const QDomNode &root, const RenderContext &context)
{
    QString html;
    for (const QDomElement &blip : elementsByName(root, QStringLiteral("blip"))) {
        const QString relationshipId = attributeByName(blip, QStringLiteral("embed"));
        const Relationship relationship = context.relationships.value(relationshipId);
        if (relationship.target.isEmpty() || relationship.external) {
            continue;
        }

        const QByteArray data = context.zip.fileData(
            cleanArchivePath(QStringLiteral("word/") + relationship.target));
        if (data.isEmpty() || data.size() > maximumDocxImageSize) {
            continue;
        }
        const EmbeddedDocumentResource resource{
            data,
            QMimeDatabase().mimeTypeForData(data).name()
        };
        const QString source = RichTextProcessor::embeddedDataUrl(resource);
        if (source.isEmpty()) {
            continue;
        }

        qreal width = 0;
        const QDomElement extent = firstElementByName(root, QStringLiteral("extent"));
        const qlonglong widthEmu = attributeByName(extent, QStringLiteral("cx")).toLongLong();
        if (widthEmu > 0) {
            width = widthEmu / 9525.0;
        }
        html += width > 0
                    ? QStringLiteral("<img src=\"%1\" width=\"%2\">")
                          .arg(source)
                          .arg(width, 0, 'f', 0)
                    : QStringLiteral("<img src=\"%1\">").arg(source);
    }
    return html;
}

QString renderRun(const QDomElement &run, const RenderContext &context)
{
    QString content;
    for (QDomNode node = run.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (element.isNull()) {
            continue;
        }

        const QString name = elementName(element);
        if (name == QLatin1String("t") || name == QLatin1String("delText")) {
            content += escaped(element.text());
        } else if (name == QLatin1String("tab")) {
            content += QStringLiteral("&emsp;");
        } else if (name == QLatin1String("br") || name == QLatin1String("cr")) {
            content += QStringLiteral("<br>");
        } else if (name == QLatin1String("drawing")
                   || name == QLatin1String("pict")) {
            content += imageHtml(element, context);
        }
    }

    const QDomElement properties = directChildElementByName(run, QStringLiteral("rPr"));
    if (!directChildElementByName(properties, QStringLiteral("b")).isNull()) {
        content = QStringLiteral("<strong>%1</strong>").arg(content);
    }
    if (!directChildElementByName(properties, QStringLiteral("i")).isNull()) {
        content = QStringLiteral("<em>%1</em>").arg(content);
    }
    const QDomElement underline = directChildElementByName(properties, QStringLiteral("u"));
    if (!underline.isNull()
        && attributeByName(underline, QStringLiteral("val")) != QLatin1String("none")) {
        content = QStringLiteral("<u>%1</u>").arg(content);
    }
    if (!directChildElementByName(properties, QStringLiteral("strike")).isNull()) {
        content = QStringLiteral("<s>%1</s>").arg(content);
    }

    const QString verticalAlign = attributeByName(
        directChildElementByName(properties, QStringLiteral("vertAlign")),
        QStringLiteral("val"));
    if (verticalAlign == QLatin1String("superscript")) {
        content = QStringLiteral("<sup>%1</sup>").arg(content);
    } else if (verticalAlign == QLatin1String("subscript")) {
        content = QStringLiteral("<sub>%1</sub>").arg(content);
    }

    QStringList styles;
    const QString color = attributeByName(
        directChildElementByName(properties, QStringLiteral("color")),
        QStringLiteral("val"));
    if (QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{6}$")).match(color).hasMatch()) {
        styles.append(QStringLiteral("color:#%1").arg(color));
    }
    const int halfPointSize = attributeByName(
                                  directChildElementByName(properties,
                                                           QStringLiteral("sz")),
                                  QStringLiteral("val"))
                                  .toInt();
    if (halfPointSize > 0) {
        styles.append(QStringLiteral("font-size:%1pt").arg(halfPointSize / 2.0));
    }
    if (!styles.isEmpty()) {
        content = QStringLiteral("<span style=\"%1\">%2</span>")
                      .arg(styles.join(u';'), content);
    }
    return content;
}

QString renderInline(const QDomNode &root, const RenderContext &context)
{
    QString html;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (element.isNull()) {
            continue;
        }

        const QString name = elementName(element);
        if (name == QLatin1String("r")) {
            html += renderRun(element, context);
        } else if (name == QLatin1String("hyperlink")) {
            const QString relationshipId = attributeByName(element, QStringLiteral("id"));
            const Relationship relationship = context.relationships.value(relationshipId);
            QString href = relationship.target;
            if (href.isEmpty()) {
                const QString anchor = attributeByName(element, QStringLiteral("anchor"));
                href = anchor.isEmpty() ? QString() : QStringLiteral("#") + anchor;
            }
            html += QStringLiteral("<a href=\"%1\">%2</a>")
                        .arg(escaped(href), renderInline(element, context));
        } else if (name == QLatin1String("fldSimple")
                   || name == QLatin1String("smartTag")
                   || name == QLatin1String("sdt")) {
            html += renderInline(element, context);
        }
    }
    return html;
}

ParagraphContent paragraphContent(const QDomElement &paragraph,
                                  const RenderContext &context)
{
    const QDomElement properties = directChildElementByName(
        paragraph, QStringLiteral("pPr"));
    const QString styleId = attributeByName(
        directChildElementByName(properties, QStringLiteral("pStyle")),
        QStringLiteral("val"));
    const StyleInfo style = context.styles.value(styleId);

    int headingLevel = style.headingLevel;
    const QString outline = attributeByName(
        directChildElementByName(properties, QStringLiteral("outlineLvl")),
        QStringLiteral("val"));
    if (!outline.isEmpty()) {
        headingLevel = qBound(1, outline.toInt() + 1, 6);
    }

    const QString alignment = attributeByName(
        directChildElementByName(properties, QStringLiteral("jc")),
        QStringLiteral("val"));
    QString alignmentStyle;
    if (alignment == QLatin1String("center")
        || alignment == QLatin1String("right")
        || alignment == QLatin1String("both")
        || alignment == QLatin1String("justify")) {
        alignmentStyle = QStringLiteral("text-align:%1")
                             .arg(alignment == QLatin1String("both")
                                      ? QStringLiteral("justify")
                                      : alignment);
    }

    return {
        renderInline(paragraph, context),
        normalizedBlock(nodeText(paragraph)),
        alignmentStyle,
        listTag(properties, context.numbering),
        headingLevel
    };
}

QString paragraphBlock(const ParagraphContent &paragraph)
{
    const QString style = paragraph.alignmentStyle.isEmpty()
                              ? QString()
                              : QStringLiteral(" style=\"%1\"")
                                    .arg(paragraph.alignmentStyle);
    if (paragraph.headingLevel > 0) {
        return QStringLiteral("<h%1%2>%3</h%1>")
            .arg(paragraph.headingLevel)
            .arg(style, paragraph.innerHtml);
    }
    return QStringLiteral("<p%1>%2</p>")
        .arg(style,
             paragraph.innerHtml.isEmpty() ? QStringLiteral("<br>")
                                           : paragraph.innerHtml);
}

QString tableHtml(const QDomElement &table, const RenderContext &context)
{
    QString html = QStringLiteral(
        "<table border=\"1\" cellspacing=\"0\" cellpadding=\"6\">");
    for (const QDomElement &row : directChildElementsByName(table, QStringLiteral("tr"))) {
        html += QStringLiteral("<tr>");
        for (const QDomElement &cell
             : directChildElementsByName(row, QStringLiteral("tc"))) {
            const QString span = attributeByName(
                firstElementByName(cell, QStringLiteral("gridSpan")),
                QStringLiteral("val"));
            html += span.toInt() > 1
                        ? QStringLiteral("<td colspan=\"%1\">").arg(span)
                        : QStringLiteral("<td>");
            for (QDomNode child = cell.firstChild();
                 !child.isNull();
                 child = child.nextSibling()) {
                const QDomElement childElement = child.toElement();
                if (elementName(childElement) == QLatin1String("p")) {
                    html += paragraphBlock(paragraphContent(childElement, context));
                } else if (elementName(childElement) == QLatin1String("tbl")) {
                    html += tableHtml(childElement, context);
                }
            }
            html += QStringLiteral("</td>");
        }
        html += QStringLiteral("</tr>");
    }
    return html + QStringLiteral("</table>");
}

QString metadataValue(const QDomDocument &metadata, const QString &name)
{
    return normalizedBlock(firstElementByName(metadata, name).text());
}

} // namespace

DocxRenderedContent DocxContentRenderer::render(const ZipArchiveReader &zip,
                                                const QDomDocument &document)
{
    const RenderContext context{zip,
                                relationships(zip),
                                styles(zip),
                                numbering(zip)};
    QString bodyHtml;
    QString activeListTag;
    QStringList chapterTitles;

    const QDomElement body = firstElementByName(document, QStringLiteral("body"));
    for (QDomNode node = body.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        const QString name = elementName(element);
        if (name == QLatin1String("p")) {
            const ParagraphContent paragraph = paragraphContent(element, context);
            if (!paragraph.listTag.isEmpty()) {
                if (activeListTag != paragraph.listTag) {
                    if (!activeListTag.isEmpty()) {
                        bodyHtml += QStringLiteral("</%1>").arg(activeListTag);
                    }
                    activeListTag = paragraph.listTag;
                    bodyHtml += QStringLiteral("<%1>").arg(activeListTag);
                }
                bodyHtml += QStringLiteral("<li>%1</li>").arg(paragraph.innerHtml);
            } else {
                if (!activeListTag.isEmpty()) {
                    bodyHtml += QStringLiteral("</%1>").arg(activeListTag);
                    activeListTag.clear();
                }
                bodyHtml += paragraphBlock(paragraph);
            }
            if (paragraph.headingLevel > 0 && !paragraph.plainText.isEmpty()) {
                chapterTitles.append(paragraph.plainText);
            }
        } else if (name == QLatin1String("tbl")) {
            if (!activeListTag.isEmpty()) {
                bodyHtml += QStringLiteral("</%1>").arg(activeListTag);
                activeListTag.clear();
            }
            bodyHtml += tableHtml(element, context);
        }
    }
    if (!activeListTag.isEmpty()) {
        bodyHtml += QStringLiteral("</%1>").arg(activeListTag);
    }

    const QString html = QStringLiteral(
        "<html><head><style>"
        "h1,h2,h3,h4,h5,h6{font-weight:600;}"
        "table{border-collapse:collapse;}"
        "td,th{border:1px solid #888;vertical-align:top;}"
        "</style></head><body>%1</body></html>")
                             .arg(bodyHtml);
    const std::unique_ptr<QTextDocument> richDocument = RichTextProcessor::fromHtml(html);
    const QDomDocument metadata = archiveXml(zip, QStringLiteral("docProps/core.xml"));
    return {
        RichTextProcessor::content(*richDocument),
        metadataValue(metadata, QStringLiteral("title")),
        metadataValue(metadata, QStringLiteral("creator")),
        chapterTitles
    };
}
