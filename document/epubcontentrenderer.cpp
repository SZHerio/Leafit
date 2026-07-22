#include "epubcontentrenderer.h"

#include "../archive/ziparchivereader.h"
#include "../archive/zippathutils.h"
#include "richtextprocessor.h"
#include "xmlutils.h"

#include <QDomDocument>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QUrl>

#include <vector>

namespace {

using DocumentXml::attributeByName;
using DocumentXml::elementName;
using DocumentXml::elementsByName;
using DocumentXml::firstElementByName;
using DocumentXml::normalizedBlock;

constexpr qsizetype maximumEpubImageSize = 16 * 1024 * 1024;
constexpr qsizetype maximumEpubStyleSheetSize = 2 * 1024 * 1024;

QString decode(const QByteArray &bytes)
{
    QStringDecoder decoder(QStringConverter::Utf8);
    QString text = decoder.decode(bytes);
    return decoder.hasError() ? QString::fromLocal8Bit(bytes) : text;
}

QString chapterTitle(const QByteArray &markup,
                     const QString &path,
                     int chapterNumber)
{
    QDomDocument document;
    if (document.setContent(markup)) {
        for (int level = 1; level <= 6; ++level) {
            const QString heading = normalizedBlock(firstElementByName(
                document, QStringLiteral("h%1").arg(level)).text());
            if (!heading.isEmpty()) {
                return heading;
            }
        }
        const QString title = normalizedBlock(firstElementByName(
            document, QStringLiteral("title")).text());
        if (!title.isEmpty()) {
            return title;
        }
    }

    QString fallback = QFileInfo(path).completeBaseName();
    fallback.replace(u'_', u' ');
    fallback.replace(u'-', u' ');
    fallback = normalizedBlock(fallback);
    return fallback.isEmpty()
               ? QStringLiteral("Chapter %1").arg(chapterNumber)
               : fallback;
}

bool containsToken(const QString &values, const QString &token)
{
    return values.simplified().split(u' ', Qt::SkipEmptyParts).contains(token);
}

void appendInlineStyle(QDomElement *element, const QString &style)
{
    if (!element || element->isNull()) {
        return;
    }
    QString current = attributeByName(*element, QStringLiteral("style")).trimmed();
    if (!current.isEmpty() && !current.endsWith(u';')) {
        current += u';';
    }
    element->setAttribute(QStringLiteral("style"), current + style);
}

void copyAttributes(const QDomElement &source, QDomElement *destination)
{
    const QDomNamedNodeMap attributes = source.attributes();
    for (int index = 0; index < attributes.size(); ++index) {
        const QDomNode attribute = attributes.item(index);
        destination->setAttribute(attribute.nodeName(), attribute.nodeValue());
    }
}

void normalizeSemanticChildren(QDomDocument *document, QDomNode parent)
{
    static const QStringList blockElements = {
        QStringLiteral("article"),
        QStringLiteral("aside"),
        QStringLiteral("figure"),
        QStringLiteral("footer"),
        QStringLiteral("header"),
        QStringLiteral("main"),
        QStringLiteral("nav"),
        QStringLiteral("section")
    };

    for (QDomNode node = parent.firstChild(); !node.isNull();) {
        const QDomNode nextNode = node.nextSibling();
        if (!node.isElement()) {
            node = nextNode;
            continue;
        }

        QDomElement element = node.toElement();
        const QString originalName = elementName(element).toLower();
        const QString semanticTypes = (attributeByName(element, QStringLiteral("type"))
                                       + u' '
                                       + attributeByName(element, QStringLiteral("role")))
                                          .toLower();
        const bool footnote = containsToken(semanticTypes, QStringLiteral("footnote"))
                              || containsToken(semanticTypes,
                                               QStringLiteral("doc-footnote"))
                              || containsToken(semanticTypes, QStringLiteral("endnote"));
        const bool noteReference = originalName == QLatin1String("a")
                                   && (containsToken(semanticTypes,
                                                     QStringLiteral("noteref"))
                                       || containsToken(semanticTypes,
                                                        QStringLiteral("doc-noteref")));

        if (blockElements.contains(originalName)
            || originalName == QLatin1String("figcaption")) {
            QDomElement replacement = document->createElement(
                originalName == QLatin1String("figcaption")
                    ? QStringLiteral("p")
                    : QStringLiteral("div"));
            copyAttributes(element, &replacement);
            while (element.hasChildNodes()) {
                replacement.appendChild(element.firstChild());
            }
            parent.replaceChild(replacement, element);
            element = replacement;
        }

        if (footnote) {
            appendInlineStyle(&element,
                              QStringLiteral("font-size:90%;margin-top:12px;"
                                             "margin-bottom:12px;"));
        }
        if (noteReference) {
            appendInlineStyle(&element,
                              QStringLiteral("vertical-align:super;font-size:75%;"));
        }
        if (originalName == QLatin1String("figcaption")) {
            appendInlineStyle(&element, QStringLiteral("font-style:italic;"));
        }

        normalizeSemanticChildren(document, element);
        node = nextNode;
    }
}

QString htmlWithEmbeddedStyleSheets(const ZipArchiveReader &zip,
                                    const QString &chapterPath,
                                    const QByteArray &markup)
{
    QDomDocument document;
    if (!document.setContent(markup)) {
        return decode(markup);
    }

    normalizeSemanticChildren(&document, document.documentElement());

    for (const QDomElement &link : elementsByName(document, QStringLiteral("link"))) {
        if (!containsToken(attributeByName(link, QStringLiteral("rel")).toLower(),
                           QStringLiteral("stylesheet"))) {
            continue;
        }
        QString href = attributeByName(link, QStringLiteral("href"));
        const qsizetype fragment = href.indexOf(u'#');
        if (fragment >= 0) {
            href.truncate(fragment);
        }
        const QString stylePath = ZipPathUtils::resolve(
            ZipPathUtils::directory(chapterPath),
            QUrl::fromPercentEncoding(href.toUtf8()));
        const QByteArray styleBytes = ZipPathUtils::fileData(zip, stylePath);
        QDomNode parent = link.parentNode();
        if (styleBytes.isEmpty() || styleBytes.size() > maximumEpubStyleSheetSize) {
            parent.removeChild(link);
            continue;
        }
        QDomElement style = document.createElement(QStringLiteral("style"));
        style.appendChild(document.createTextNode(decode(styleBytes)));
        parent.replaceChild(style, link);
    }
    return document.toString(-1);
}

QString anchorPrefix(int index)
{
    return QStringLiteral("epub-%1-").arg(index);
}

QString internalLink(const QString &currentPath,
                     const QString &href,
                     const QHash<QString, int> &chapterIndexes)
{
    QString target = href.trimmed();
    if (target.isEmpty()) {
        return {};
    }

    const QUrl url(target);
    if (!url.scheme().isEmpty() || target.startsWith(QStringLiteral("//"))) {
        return target;
    }

    QString fragment;
    const qsizetype fragmentSeparator = target.indexOf(u'#');
    if (fragmentSeparator >= 0) {
        fragment = QUrl::fromPercentEncoding(
            target.mid(fragmentSeparator + 1).toUtf8());
        target.truncate(fragmentSeparator);
    }
    const qsizetype querySeparator = target.indexOf(u'?');
    if (querySeparator >= 0) {
        target.truncate(querySeparator);
    }

    const QString targetPath = target.isEmpty()
                                   ? currentPath
                                   : ZipPathUtils::resolve(
                                         ZipPathUtils::directory(currentPath),
                                         QUrl::fromPercentEncoding(target.toUtf8()));
    if (!chapterIndexes.contains(targetPath)) {
        return {};
    }

    const QString localAnchor = fragment.isEmpty() ? QStringLiteral("start") : fragment;
    return QStringLiteral("#%1%2")
        .arg(anchorPrefix(chapterIndexes.value(targetPath)), localAnchor);
}

EmbeddedDocumentResource chapterImage(const ZipArchiveReader &zip,
                                      const QString &chapterPath,
                                      QString source)
{
    if (source.isEmpty() || source.startsWith(QStringLiteral("data:"),
                                               Qt::CaseInsensitive)) {
        return {};
    }
    const QUrl sourceUrl(source);
    if (!sourceUrl.scheme().isEmpty() || source.startsWith(QStringLiteral("//"))) {
        return {};
    }

    const qsizetype fragment = source.indexOf(u'#');
    if (fragment >= 0) {
        source.truncate(fragment);
    }
    const qsizetype query = source.indexOf(u'?');
    if (query >= 0) {
        source.truncate(query);
    }
    const QString imagePath = ZipPathUtils::resolve(
        ZipPathUtils::directory(chapterPath),
        QUrl::fromPercentEncoding(source.toUtf8()));
    const QByteArray data = ZipPathUtils::fileData(zip, imagePath);
    if (data.isEmpty() || data.size() > maximumEpubImageSize) {
        return {};
    }
    return {data, QMimeDatabase().mimeTypeForData(data).name()};
}

} // namespace

EpubRenderedContent EpubContentRenderer::render(
    const ZipArchiveReader &zip,
    const QStringList &chapterPaths)
{
    QHash<QString, int> chapterIndexes;
    for (int index = 0; index < chapterPaths.size(); ++index) {
        chapterIndexes.insert(chapterPaths.at(index), index);
    }

    std::vector<std::unique_ptr<QTextDocument>> documents;
    QVector<EpubRenderedChapter> chapters;
    documents.reserve(chapterPaths.size());
    chapters.reserve(chapterPaths.size());

    int chapterNumber = 1;
    for (int index = 0; index < chapterPaths.size(); ++index) {
        const QString chapterPath = chapterPaths.at(index);
        const QByteArray markup = ZipPathUtils::fileData(zip, chapterPath);
        if (markup.isEmpty()) {
            continue;
        }

        RichTextProcessor::Options options;
        options.anchorPrefix = anchorPrefix(index);
        options.startAnchor = anchorPrefix(index) + QStringLiteral("start");
        options.imageResolver = [&zip, chapterPath](const QString &source) {
            return chapterImage(zip, chapterPath, source);
        };
        options.linkResolver = [chapterPath, chapterIndexes](const QString &href) {
            return internalLink(chapterPath, href, chapterIndexes);
        };

        std::unique_ptr<QTextDocument> document = RichTextProcessor::fromHtml(
            htmlWithEmbeddedStyleSheets(zip, chapterPath, markup), options);
        const RichTextContent chapterContent = RichTextProcessor::content(*document);
        if (chapterContent.isEmpty()) {
            continue;
        }

        chapters.append({chapterPath,
                         chapterTitle(markup, chapterPath, chapterNumber),
                         chapterContent.plainText,
                         markup,
                         0});
        documents.push_back(std::move(document));
        ++chapterNumber;
    }

    QTextDocument mergedDocument;
    QTextCursor cursor(&mergedDocument);
    bool firstDocument = true;
    for (const std::unique_ptr<QTextDocument> &document : documents) {
        if (!firstDocument) {
            cursor.movePosition(QTextCursor::End);
            cursor.insertBlock();
            cursor.insertBlock();
        }
        cursor.movePosition(QTextCursor::End);
        cursor.insertFragment(QTextDocumentFragment(document.get()));
        firstDocument = false;
    }

    RichTextContent content = RichTextProcessor::content(mergedDocument);
    qsizetype searchOffset = 0;
    for (EpubRenderedChapter &chapter : chapters) {
        const qsizetype offset = content.plainText.indexOf(chapter.plainText,
                                                           searchOffset);
        chapter.offset = qMax(qsizetype(0), offset);
        searchOffset = chapter.offset + chapter.plainText.size();
    }
    return {content, chapters};
}
