#include "documentreaders.h"

#include "archive/ziparchivereader.h"
#include "archive/zippathutils.h"
#include "document/docxcontentrenderer.h"
#include "document/documentlimits.h"
#include "document/epubcontentrenderer.h"
#include "document/fb2contentrenderer.h"
#include "document/richtextprocessor.h"
#include "document/xmlutils.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QPdfDocument>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTextDocument>

#include <memory>

namespace {

[[maybe_unused]] const char *const documentReaderTranslationSources[] = {
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not read the file."),
    QT_TRANSLATE_NOOP("DocumentReaders", "This document is too large. The limit is 128 MB."),
    QT_TRANSLATE_NOOP("DocumentReaders", "PDF file not found."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Invalid PDF file."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Incorrect PDF password."),
    QT_TRANSLATE_NOOP("DocumentReaders", "This PDF security scheme is not supported."),
    QT_TRANSLATE_NOOP("DocumentReaders", "PDF data is not available yet."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not open PDF."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not parse FB2: %1 at %2:%3."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not open EPUB archive."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not find EPUB package file."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not read EPUB package file."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not parse EPUB package: %1 at %2:%3."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not extract readable text from EPUB."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not open DOCX archive."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not find DOCX document body."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not parse DOCX: %1 at %2:%3."),
    QT_TRANSLATE_NOOP("DocumentReaders", "Could not extract readable content from DOCX.")
};

using DocumentXml::attributeByName;
using DocumentXml::directChildElementByName;
using DocumentXml::directChildElementsByName;
using DocumentXml::elementsByName;
using DocumentXml::firstElementByName;
using DocumentXml::nodeText;
using DocumentXml::normalizedBlock;

struct ByteLoadResult
{
    QByteArray bytes;
    QString errorMessage;

    bool isSuccess() const
    {
        return errorMessage.isEmpty();
    }
};

QString trDocument(const char *sourceText)
{
    return QCoreApplication::translate("DocumentReaders", sourceText);
}

QString decodeText(const QByteArray &bytes)
{
    QStringDecoder utf8Decoder(QStringConverter::Utf8);
    QString text = utf8Decoder.decode(bytes);
    if (utf8Decoder.hasError()) {
        text = QString::fromLocal8Bit(bytes);
    }
    return text;
}

ByteLoadResult readBytes(const QFileInfo &fileInfo)
{
    if (fileInfo.size() > DocumentLimits::maximumTextSourceBytes) {
        return {{}, trDocument("This document is too large. The limit is 128 MB.")};
    }

    QFile file(fileInfo.absoluteFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return {{}, file.errorString()};
    }

    ByteLoadResult result;
    result.bytes = file.readAll();
    if (file.error() != QFile::NoError) {
        result.errorMessage = trDocument("Could not read the file.");
    }

    return result;
}

struct ChapterOffset final
{
    QString title;
    qsizetype offset = 0;
};

QVector<DocumentChapter> normalizedChapters(const QString &text,
                                            const QVector<ChapterOffset> &chapterOffsets)
{
    QVector<DocumentChapter> chapters;
    chapters.reserve(chapterOffsets.size());
    const qreal textLength = qMax(qsizetype(1), text.size());

    for (const ChapterOffset &chapterOffset : chapterOffsets) {
        const QString title = normalizedBlock(chapterOffset.title);
        if (title.isEmpty()) {
            continue;
        }

        const qreal progress = qBound(qreal(0), chapterOffset.offset / textLength, qreal(1));
        if (!chapters.isEmpty()
            && chapters.constLast().title == title
            && qFuzzyCompare(chapters.constLast().progress + 1, progress + 1)) {
            continue;
        }
        chapters.append({title, progress});
    }

    return chapters;
}

QVector<DocumentChapter> chaptersFromTitles(const QString &text, const QStringList &titles)
{
    QVector<ChapterOffset> chapterOffsets;
    qsizetype searchOffset = 0;
    for (const QString &title : titles) {
        const QString normalizedTitle = normalizedBlock(title);
        if (normalizedTitle.isEmpty()) {
            continue;
        }

        const qsizetype titleOffset = text.indexOf(normalizedTitle,
                                                    searchOffset,
                                                    Qt::CaseInsensitive);
        if (titleOffset < 0) {
            continue;
        }

        chapterOffsets.append({normalizedTitle, titleOffset});
        searchOffset = titleOffset + normalizedTitle.size();
    }

    return normalizedChapters(text, chapterOffsets);
}

QString joinedElementValues(const QDomNode &root, const QString &name)
{
    QStringList values;
    for (const QDomElement &element : elementsByName(root, name)) {
        const QString value = normalizedBlock(element.text());
        if (!value.isEmpty() && !values.contains(value)) {
            values.append(value);
        }
    }
    return values.join(QStringLiteral(", "));
}

QString fb2Author(const QDomElement &titleInfo)
{
    QStringList authors;
    for (const QDomElement &authorElement
         : directChildElementsByName(titleInfo, QStringLiteral("author"))) {
        QStringList nameParts;
        static const QStringList partNames = {
            QStringLiteral("first-name"),
            QStringLiteral("middle-name"),
            QStringLiteral("last-name")
        };
        for (const QString &partName : partNames) {
            const QString part = normalizedBlock(
                directChildElementByName(authorElement, partName).text());
            if (!part.isEmpty()) {
                nameParts.append(part);
            }
        }

        QString author = nameParts.join(u' ');
        if (author.isEmpty()) {
            author = normalizedBlock(
                directChildElementByName(authorElement, QStringLiteral("nickname")).text());
        }
        if (!author.isEmpty() && !authors.contains(author)) {
            authors.append(author);
        }
    }
    return authors.join(QStringLiteral(", "));
}

struct EpubManifestItem final
{
    QString href;
    QString mediaType;
    QString properties;
};

struct EpubReference final
{
    QString path;
    QString fragment;
    bool valid = false;
};

struct EpubTocEntry final
{
    QString title;
    QString path;
    QString fragment;
};

bool containsToken(const QString &values, const QString &token)
{
    return values.simplified().split(u' ', Qt::SkipEmptyParts).contains(token);
}

QString packageItemPath(const QString &packageDirectory, QString href)
{
    const qsizetype fragment = href.indexOf(u'#');
    if (fragment >= 0) {
        href.truncate(fragment);
    }
    const qsizetype query = href.indexOf(u'?');
    if (query >= 0) {
        href.truncate(query);
    }
    href = QUrl::fromPercentEncoding(href.toUtf8());
    return ZipPathUtils::resolve(packageDirectory, href);
}

EpubReference resolveEpubReference(const QString &documentPath, QString href)
{
    href = href.trimmed();
    if (href.isEmpty() || href.startsWith(QStringLiteral("//"))) {
        return {};
    }

    const qsizetype fragmentSeparator = href.indexOf(u'#');
    QString fragment;
    if (fragmentSeparator >= 0) {
        fragment = QUrl::fromPercentEncoding(href.mid(fragmentSeparator + 1).toUtf8());
        href.truncate(fragmentSeparator);
    }

    const qsizetype querySeparator = href.indexOf(u'?');
    if (querySeparator >= 0) {
        href.truncate(querySeparator);
    }

    const qsizetype schemeSeparator = href.indexOf(u':');
    const qsizetype pathSeparator = href.indexOf(u'/');
    if (schemeSeparator >= 0
        && (pathSeparator < 0 || schemeSeparator < pathSeparator)) {
        return {};
    }

    href = QUrl::fromPercentEncoding(href.toUtf8());
    const QString path = href.isEmpty()
                             ? documentPath
                             : ZipPathUtils::resolve(ZipPathUtils::directory(documentPath), href);
    return {path, fragment, !path.isEmpty()};
}

QVector<EpubTocEntry> epub3Navigation(const ZipArchiveReader &zip,
                                      const QHash<QString, EpubManifestItem> &manifest,
                                      const QString &packageDirectory)
{
    EpubManifestItem navigationItem;
    for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
        if (containsToken(item->properties, QStringLiteral("nav"))) {
            navigationItem = item.value();
            break;
        }
    }
    if (navigationItem.href.isEmpty()) {
        return {};
    }

    const QString navigationPath = packageItemPath(packageDirectory, navigationItem.href);
    const QByteArray navigationBytes = ZipPathUtils::fileData(zip, navigationPath);
    QDomDocument navigationDocument;
    if (navigationBytes.isEmpty() || !navigationDocument.setContent(navigationBytes)) {
        return {};
    }

    const QList<QDomElement> navigationElements = elementsByName(
        navigationDocument, QStringLiteral("nav"));
    QDomElement tocNavigation;
    for (const QDomElement &navigation : navigationElements) {
        if (containsToken(attributeByName(navigation, QStringLiteral("type")),
                          QStringLiteral("toc"))) {
            tocNavigation = navigation;
            break;
        }
    }
    if (tocNavigation.isNull() && !navigationElements.isEmpty()) {
        tocNavigation = navigationElements.constFirst();
    }

    QVector<EpubTocEntry> entries;
    for (const QDomElement &anchor : elementsByName(tocNavigation, QStringLiteral("a"))) {
        const QString title = normalizedBlock(nodeText(anchor));
        const EpubReference reference = resolveEpubReference(
            navigationPath, attributeByName(anchor, QStringLiteral("href")));
        if (!title.isEmpty() && reference.valid) {
            entries.append({title, reference.path, reference.fragment});
        }
    }
    return entries;
}

QVector<EpubTocEntry> epub2Navigation(const ZipArchiveReader &zip,
                                      const QDomDocument &packageDocument,
                                      const QHash<QString, EpubManifestItem> &manifest,
                                      const QString &packageDirectory)
{
    EpubManifestItem navigationItem;
    const QDomElement spine = firstElementByName(packageDocument, QStringLiteral("spine"));
    const QString navigationId = attributeByName(spine, QStringLiteral("toc"));
    if (!navigationId.isEmpty()) {
        navigationItem = manifest.value(navigationId);
    }
    if (navigationItem.href.isEmpty()) {
        for (auto item = manifest.cbegin(); item != manifest.cend(); ++item) {
            if (item->mediaType == QLatin1String("application/x-dtbncx+xml")) {
                navigationItem = item.value();
                break;
            }
        }
    }
    if (navigationItem.href.isEmpty()) {
        return {};
    }

    const QString navigationPath = packageItemPath(packageDirectory, navigationItem.href);
    const QByteArray navigationBytes = ZipPathUtils::fileData(zip, navigationPath);
    QDomDocument navigationDocument;
    if (navigationBytes.isEmpty() || !navigationDocument.setContent(navigationBytes)) {
        return {};
    }

    QVector<EpubTocEntry> entries;
    for (const QDomElement &navigationPoint
         : elementsByName(navigationDocument, QStringLiteral("navPoint"))) {
        const QDomElement labelElement = directChildElementByName(
            navigationPoint, QStringLiteral("navLabel"));
        const QDomElement contentElement = directChildElementByName(
            navigationPoint, QStringLiteral("content"));
        const QString title = normalizedBlock(
            firstElementByName(labelElement, QStringLiteral("text")).text());
        const EpubReference reference = resolveEpubReference(
            navigationPath, attributeByName(contentElement, QStringLiteral("src")));
        if (!title.isEmpty() && reference.valid) {
            entries.append({title, reference.path, reference.fragment});
        }
    }
    return entries;
}

QDomElement elementWithAnchor(const QDomNode &root, const QString &anchor)
{
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull()
            && (attributeByName(element, QStringLiteral("id")) == anchor
                || attributeByName(element, QStringLiteral("name")) == anchor)) {
            return element;
        }

        const QDomElement nested = elementWithAnchor(node, anchor);
        if (!nested.isNull()) {
            return nested;
        }
    }
    return {};
}

qsizetype tocOffsetInChapter(const EpubTocEntry &entry,
                             const EpubRenderedChapter &chapter)
{
    QString anchorText;
    if (!entry.fragment.isEmpty()) {
        QDomDocument document;
        if (document.setContent(chapter.markup)) {
            anchorText = normalizedBlock(
                nodeText(elementWithAnchor(document, entry.fragment)));
        }
    }

    qsizetype localOffset = -1;
    if (!anchorText.isEmpty()) {
        localOffset = chapter.plainText.indexOf(anchorText, 0, Qt::CaseInsensitive);
    }
    if (localOffset < 0) {
        localOffset = chapter.plainText.indexOf(entry.title, 0, Qt::CaseInsensitive);
    }
    return chapter.offset + qMax(qsizetype(0), localOffset);
}

QVector<ChapterOffset> epubChapterOffsets(const QVector<EpubRenderedChapter> &chapters,
                                          const QVector<EpubTocEntry> &tocEntries)
{
    QVector<ChapterOffset> offsets;
    for (const EpubRenderedChapter &chapter : chapters) {
        bool hasTocEntry = false;
        for (const EpubTocEntry &entry : tocEntries) {
            if (entry.path != chapter.path) {
                continue;
            }
            offsets.append({entry.title, tocOffsetInChapter(entry, chapter)});
            hasTocEntry = true;
        }
        if (!hasTocEntry) {
            offsets.append({chapter.title, chapter.offset});
        }
    }
    return offsets;
}

QString pdfErrorText(QPdfDocument::Error error)
{
    switch (error) {
    case QPdfDocument::Error::None:
        return {};
    case QPdfDocument::Error::FileNotFound:
        return trDocument("PDF file not found.");
    case QPdfDocument::Error::InvalidFileFormat:
        return trDocument("Invalid PDF file.");
    case QPdfDocument::Error::IncorrectPassword:
        return trDocument("Incorrect PDF password.");
    case QPdfDocument::Error::UnsupportedSecurityScheme:
        return trDocument("This PDF security scheme is not supported.");
    case QPdfDocument::Error::DataNotYetAvailable:
        return trDocument("PDF data is not available yet.");
    case QPdfDocument::Error::Unknown:
        return trDocument("Could not open PDF.");
    }

    return trDocument("Could not open PDF.");
}

} // namespace

QStringList PlainTextDocumentReader::suffixes() const
{
    return {QStringLiteral("txt")};
}

DocumentLoadResult PlainTextDocumentReader::load(const QFileInfo &fileInfo) const
{
    const ByteLoadResult bytes = readBytes(fileInfo);
    if (!bytes.isSuccess()) {
        return DocumentLoadResult::failure(bytes.errorMessage);
    }

    return DocumentLoadResult::textDocument(fileInfo, QStringLiteral("TXT"), decodeText(bytes.bytes));
}

QStringList HtmlDocumentReader::suffixes() const
{
    return {QStringLiteral("html"), QStringLiteral("htm")};
}

DocumentLoadResult HtmlDocumentReader::load(const QFileInfo &fileInfo) const
{
    const ByteLoadResult bytes = readBytes(fileInfo);
    if (!bytes.isSuccess()) {
        return DocumentLoadResult::failure(bytes.errorMessage);
    }

    RichTextProcessor::Options options;
    options.baseUrl = QUrl::fromLocalFile(fileInfo.absolutePath() + u'/');
    const QUrl documentUrl = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
    options.linkResolver = [documentUrl, baseUrl = options.baseUrl](const QString &href) {
        const QString trimmedHref = href.trimmed();
        if (trimmedHref.isEmpty() || trimmedHref.startsWith(u'#')) {
            return trimmedHref;
        }

        QUrl resolved(trimmedHref);
        if (resolved.isRelative()) {
            resolved = baseUrl.resolved(resolved);
        }
        if (resolved.isLocalFile()
            && QFileInfo(resolved.toLocalFile()).absoluteFilePath()
                   == QFileInfo(documentUrl.toLocalFile()).absoluteFilePath()) {
            const QString fragment = resolved.fragment(QUrl::FullyDecoded);
            return fragment.isEmpty() ? QStringLiteral("#") : u'#' + fragment;
        }
        return resolved.toString(QUrl::FullyEncoded);
    };
    const std::unique_ptr<QTextDocument> document = RichTextProcessor::fromHtml(
        decodeText(bytes.bytes), options);
    const RichTextContent content = RichTextProcessor::content(*document);
    return DocumentLoadResult::richTextDocument(fileInfo,
                                                QStringLiteral("HTML"),
                                                content.plainText,
                                                content.html,
                                                content.title);
}

QStringList MarkdownDocumentReader::suffixes() const
{
    return {QStringLiteral("md"), QStringLiteral("markdown")};
}

DocumentLoadResult MarkdownDocumentReader::load(const QFileInfo &fileInfo) const
{
    const ByteLoadResult bytes = readBytes(fileInfo);
    if (!bytes.isSuccess()) {
        return DocumentLoadResult::failure(bytes.errorMessage);
    }

    RichTextProcessor::Options options;
    options.baseUrl = QUrl::fromLocalFile(fileInfo.absolutePath() + u'/');
    const std::unique_ptr<QTextDocument> document = RichTextProcessor::fromMarkdown(
        decodeText(bytes.bytes), options);
    const RichTextContent content = RichTextProcessor::content(*document);
    return DocumentLoadResult::richTextDocument(fileInfo,
                                                QStringLiteral("Markdown"),
                                                content.plainText,
                                                content.html);
}

QStringList Fb2DocumentReader::suffixes() const
{
    return {QStringLiteral("fb2")};
}

DocumentLoadResult Fb2DocumentReader::load(const QFileInfo &fileInfo) const
{
    const ByteLoadResult bytes = readBytes(fileInfo);
    if (!bytes.isSuccess()) {
        return DocumentLoadResult::failure(bytes.errorMessage);
    }

    QDomDocument document;
    QString parseError;
    int errorLine = 0;
    int errorColumn = 0;
    if (!document.setContent(bytes.bytes, &parseError, &errorLine, &errorColumn)) {
        return DocumentLoadResult::failure(
            trDocument("Could not parse FB2: %1 at %2:%3.").arg(parseError).arg(errorLine).arg(errorColumn));
    }

    const QDomElement description = firstElementByName(document, QStringLiteral("description"));
    const QDomElement titleInfo = firstElementByName(description, QStringLiteral("title-info"));
    const QDomElement titleElement = directChildElementByName(
        titleInfo, QStringLiteral("book-title"));
    const Fb2RenderedContent rendered = Fb2ContentRenderer::render(document);
    return DocumentLoadResult::richTextDocument(
        fileInfo,
        QStringLiteral("FB2"),
        rendered.richText.plainText,
        rendered.richText.html,
        normalizedBlock(titleElement.text()),
        fb2Author(titleInfo),
        chaptersFromTitles(rendered.richText.plainText,
                           rendered.chapterTitles));
}

QStringList EpubDocumentReader::suffixes() const
{
    return {QStringLiteral("epub")};
}

DocumentLoadResult EpubDocumentReader::load(const QFileInfo &fileInfo) const
{
    const ZipArchiveReader zip(fileInfo.absoluteFilePath());
    if (!zip.isOpen()) {
        return DocumentLoadResult::failure(trDocument("Could not open EPUB archive."));
    }

    QString opfPath;
    const QByteArray containerXml = ZipPathUtils::fileData(
        zip, QStringLiteral("META-INF/container.xml"));
    if (!containerXml.isEmpty()) {
        QDomDocument container;
        if (container.setContent(containerXml)) {
            const QDomElement rootfile = firstElementByName(container, QStringLiteral("rootfile"));
            opfPath = ZipPathUtils::clean(QUrl::fromPercentEncoding(
                attributeByName(rootfile, QStringLiteral("full-path")).toUtf8()));
        }
    }

    if (opfPath.isEmpty()) {
        opfPath = ZipPathUtils::firstFileWithSuffix(zip, QStringLiteral(".opf"));
    }

    if (opfPath.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not find EPUB package file."));
    }

    const QByteArray opfBytes = ZipPathUtils::fileData(zip, opfPath);
    if (opfBytes.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not read EPUB package file."));
    }

    QDomDocument opf;
    QString parseError;
    int errorLine = 0;
    int errorColumn = 0;
    if (!opf.setContent(opfBytes, &parseError, &errorLine, &errorColumn)) {
        return DocumentLoadResult::failure(
            trDocument("Could not parse EPUB package: %1 at %2:%3.")
                .arg(parseError)
                .arg(errorLine)
                .arg(errorColumn));
    }

    QHash<QString, EpubManifestItem> manifest;
    for (const QDomElement &item : elementsByName(opf, QStringLiteral("item"))) {
        manifest.insert(attributeByName(item, QStringLiteral("id")),
                        {attributeByName(item, QStringLiteral("href")),
                         attributeByName(item, QStringLiteral("media-type")),
                         attributeByName(item, QStringLiteral("properties"))});
    }

    const QString basePath = ZipPathUtils::directory(opfPath);
    QStringList chapterPaths;
    QStringList auxiliaryChapterPaths;
    const QDomElement spine = firstElementByName(opf, QStringLiteral("spine"));
    for (const QDomElement &itemref : elementsByName(spine, QStringLiteral("itemref"))) {
        const EpubManifestItem item = manifest.value(
            attributeByName(itemref, QStringLiteral("idref")));
        const QString chapterPath = packageItemPath(basePath, item.href);
        if (chapterPath.isEmpty()
            || chapterPaths.contains(chapterPath)
            || auxiliaryChapterPaths.contains(chapterPath)) {
            continue;
        }
        if (attributeByName(itemref, QStringLiteral("linear")).compare(
                QStringLiteral("no"), Qt::CaseInsensitive) == 0) {
            auxiliaryChapterPaths.append(chapterPath);
        } else {
            chapterPaths.append(chapterPath);
        }
    }
    chapterPaths.append(auxiliaryChapterPaths);

    if (chapterPaths.isEmpty()) {
        for (const EpubManifestItem &item : manifest) {
            if (item.mediaType == QLatin1String("application/xhtml+xml")
                || item.mediaType == QLatin1String("text/html")) {
                const QString chapterPath = packageItemPath(basePath, item.href);
                if (!chapterPath.isEmpty()
                    && !containsToken(item.properties, QStringLiteral("nav"))) {
                    chapterPaths.append(chapterPath);
                }
            }
        }
        chapterPaths.sort(Qt::CaseInsensitive);
    }

    QVector<EpubTocEntry> tocEntries = epub3Navigation(zip, manifest, basePath);
    if (tocEntries.isEmpty()) {
        tocEntries = epub2Navigation(zip, opf, manifest, basePath);
    }

    const EpubRenderedContent rendered = EpubContentRenderer::render(zip, chapterPaths);
    if (rendered.richText.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not extract readable text from EPUB."));
    }

    const QDomElement metadata = firstElementByName(opf, QStringLiteral("metadata"));
    const QDomElement titleElement = firstElementByName(metadata, QStringLiteral("title"));
    return DocumentLoadResult::richTextDocument(
        fileInfo,
        QStringLiteral("EPUB"),
        rendered.richText.plainText,
        rendered.richText.html,
        normalizedBlock(titleElement.text()),
        joinedElementValues(metadata, QStringLiteral("creator")),
        normalizedChapters(rendered.richText.plainText,
                           epubChapterOffsets(rendered.chapters, tocEntries)));
}

QStringList DocxDocumentReader::suffixes() const
{
    return {QStringLiteral("docx")};
}

DocumentLoadResult DocxDocumentReader::load(const QFileInfo &fileInfo) const
{
    const ZipArchiveReader zip(fileInfo.absoluteFilePath());
    if (!zip.isOpen()) {
        return DocumentLoadResult::failure(trDocument("Could not open DOCX archive."));
    }

    const QByteArray documentXml = ZipPathUtils::fileData(
        zip, QStringLiteral("word/document.xml"));
    if (documentXml.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not find DOCX document body."));
    }

    QDomDocument document;
    QString parseError;
    int errorLine = 0;
    int errorColumn = 0;
    if (!document.setContent(documentXml, &parseError, &errorLine, &errorColumn)) {
        return DocumentLoadResult::failure(
            trDocument("Could not parse DOCX: %1 at %2:%3.").arg(parseError).arg(errorLine).arg(errorColumn));
    }

    const DocxRenderedContent rendered = DocxContentRenderer::render(zip, document);
    if (rendered.richText.isEmpty()) {
        return DocumentLoadResult::failure(
            trDocument("Could not extract readable content from DOCX."));
    }
    return DocumentLoadResult::richTextDocument(
        fileInfo,
        QStringLiteral("DOCX"),
        rendered.richText.plainText,
        rendered.richText.html,
        rendered.title,
        rendered.author,
        chaptersFromTitles(rendered.richText.plainText,
                           rendered.chapterTitles));
}

QStringList PdfDocumentReader::suffixes() const
{
    return {QStringLiteral("pdf")};
}

DocumentLoadResult PdfDocumentReader::load(const QFileInfo &fileInfo) const
{
    QPdfDocument document;
    const QPdfDocument::Error error = document.load(fileInfo.absoluteFilePath());
    if (error == QPdfDocument::Error::IncorrectPassword) {
        return DocumentLoadResult::pdfDocument(fileInfo, QStringLiteral("PDF"));
    }

    const QString errorText = pdfErrorText(error);
    if (!errorText.isEmpty()) {
        return DocumentLoadResult::failure(errorText);
    }

    const QString title = document.metaData(QPdfDocument::MetaDataField::Title).toString().trimmed();
    const QString author = document.metaData(QPdfDocument::MetaDataField::Author).toString().trimmed();
    return DocumentLoadResult::pdfDocument(fileInfo, QStringLiteral("PDF"), title, author);
}
