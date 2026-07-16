#include "documentreaders.h"

#include "archive/ziparchivereader.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QPdfDocument>
#include <QSet>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTextDocument>

namespace {

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

QString elementName(const QDomElement &element)
{
    QString name = element.localName();
    if (name.isEmpty()) {
        name = element.tagName();
    }

    const qsizetype separator = name.indexOf(u':');
    if (separator >= 0) {
        name = name.mid(separator + 1);
    }

    return name;
}

QDomElement firstElementByName(const QDomNode &root, const QString &name)
{
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            return element;
        }

        const QDomElement nested = firstElementByName(node, name);
        if (!nested.isNull()) {
            return nested;
        }
    }

    return {};
}

QDomElement directChildElementByName(const QDomNode &root, const QString &name)
{
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            return element;
        }
    }

    return {};
}

QList<QDomElement> directChildElementsByName(const QDomNode &root, const QString &name)
{
    QList<QDomElement> elements;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            elements.append(element);
        }
    }
    return elements;
}

QList<QDomElement> elementsByName(const QDomNode &root, const QString &name)
{
    QList<QDomElement> elements;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            elements.append(element);
        }

        elements.append(elementsByName(node, name));
    }

    return elements;
}

QString attributeByName(const QDomElement &element, const QString &name)
{
    const QDomNamedNodeMap attributes = element.attributes();
    for (int index = 0; index < attributes.count(); ++index) {
        const QDomAttr attribute = attributes.item(index).toAttr();
        QString attributeName = attribute.localName();
        if (attributeName.isEmpty()) {
            attributeName = attribute.name();
        }

        const qsizetype separator = attributeName.indexOf(u':');
        if (separator >= 0) {
            attributeName = attributeName.mid(separator + 1);
        }

        if (attributeName == name) {
            return attribute.value();
        }
    }
    return {};
}

QString normalizedBlock(QString text)
{
    text.replace(QChar::Nbsp, u' ');
    return text.simplified();
}

QString xmlNodeText(const QDomNode &root)
{
    QString text;

    if (root.isText() || root.isCDATASection()) {
        return root.nodeValue();
    }

    const QDomElement element = root.toElement();
    if (!element.isNull()) {
        const QString name = elementName(element);
        if (name == QLatin1String("br") || name == QLatin1String("empty-line")) {
            return QStringLiteral("\n");
        }
    }

    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        text += xmlNodeText(node);
    }

    return text;
}

void collectFb2Blocks(const QDomNode &root, QStringList *blocks)
{
    static const QSet<QString> blockElements = {
        QStringLiteral("p"),
        QStringLiteral("subtitle"),
        QStringLiteral("title"),
        QStringLiteral("v"),
        QStringLiteral("text-author"),
        QStringLiteral("date")
    };

    const QDomElement element = root.toElement();
    if (!element.isNull()) {
        const QString name = elementName(element);
        if (name == QLatin1String("empty-line")) {
            blocks->append(QString());
            return;
        }

        if (blockElements.contains(name)) {
            const QString block = normalizedBlock(xmlNodeText(element));
            if (!block.isEmpty()) {
                blocks->append(block);
            }
            return;
        }
    }

    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        collectFb2Blocks(node, blocks);
    }
}

QString textFromHtml(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    return document.toPlainText().trimmed();
}

QString textFromMarkdown(const QString &markdown)
{
    QTextDocument document;
    document.setMarkdown(markdown);
    return document.toPlainText().trimmed();
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

QString chapterTitleFromHtml(const QByteArray &htmlBytes,
                             const QString &fallbackPath,
                             int chapterNumber)
{
    QDomDocument document;
    if (document.setContent(htmlBytes)) {
        static const QStringList headingNames = {
            QStringLiteral("h1"),
            QStringLiteral("h2"),
            QStringLiteral("h3"),
            QStringLiteral("h4"),
            QStringLiteral("h5"),
            QStringLiteral("h6")
        };
        for (const QString &headingName : headingNames) {
            const QString heading = normalizedBlock(firstElementByName(document, headingName).text());
            if (!heading.isEmpty()) {
                return heading;
            }
        }

        const QString documentTitle = normalizedBlock(
            firstElementByName(document, QStringLiteral("title")).text());
        if (!documentTitle.isEmpty()) {
            return documentTitle;
        }
    }

    QString fallbackTitle = QFileInfo(fallbackPath).completeBaseName();
    fallbackTitle.replace(u'_', u' ');
    fallbackTitle.replace(u'-', u' ');
    fallbackTitle = normalizedBlock(fallbackTitle);
    return fallbackTitle.isEmpty()
               ? trDocument("Chapter %1").arg(chapterNumber)
               : fallbackTitle;
}

QString cleanZipPath(const QString &path)
{
    const QString normalized = QString(path).replace(u'\\', u'/');
    QStringList cleanParts;

    for (const QString &part : normalized.split(u'/', Qt::SkipEmptyParts)) {
        if (part == QLatin1String(".")) {
            continue;
        }

        if (part == QLatin1String("..")) {
            if (!cleanParts.isEmpty()) {
                cleanParts.removeLast();
            }
            continue;
        }

        cleanParts.append(part);
    }

    return cleanParts.join(u'/');
}

QString zipDirectory(const QString &path)
{
    const qsizetype separator = path.lastIndexOf(u'/');
    if (separator < 0) {
        return {};
    }
    return path.left(separator);
}

QString resolveZipPath(const QString &basePath, const QString &href)
{
    if (href.startsWith(u'/')) {
        return cleanZipPath(href.mid(1));
    }

    if (basePath.isEmpty()) {
        return cleanZipPath(href);
    }

    return cleanZipPath(basePath + u'/' + href);
}

QByteArray zipFileData(const ZipArchiveReader &zip, const QString &path)
{
    QByteArray data = zip.fileData(path);
    if (!data.isEmpty()) {
        return data;
    }

    const QString decodedPath = cleanZipPath(QUrl::fromPercentEncoding(path.toUtf8()));
    if (decodedPath != path) {
        data = zip.fileData(decodedPath);
    }

    return data;
}

QString firstZipFileWithSuffix(const ZipArchiveReader &zip, const QString &suffix)
{
    for (const QString &path : zip.filePaths()) {
        if (path.endsWith(suffix, Qt::CaseInsensitive)) {
            return path;
        }
    }

    return {};
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

struct EpubChapterContent final
{
    QString path;
    QString title;
    QString text;
    QByteArray markup;
    qsizetype offset = 0;
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
    return resolveZipPath(packageDirectory, href);
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
                             : resolveZipPath(zipDirectory(documentPath), href);
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
    const QByteArray navigationBytes = zipFileData(zip, navigationPath);
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
        const QString title = normalizedBlock(xmlNodeText(anchor));
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
    const QByteArray navigationBytes = zipFileData(zip, navigationPath);
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
                             const EpubChapterContent &chapter)
{
    QString anchorText;
    if (!entry.fragment.isEmpty()) {
        QDomDocument document;
        if (document.setContent(chapter.markup)) {
            anchorText = normalizedBlock(
                xmlNodeText(elementWithAnchor(document, entry.fragment)));
        }
    }

    qsizetype localOffset = -1;
    if (!anchorText.isEmpty()) {
        localOffset = chapter.text.indexOf(anchorText, 0, Qt::CaseInsensitive);
    }
    if (localOffset < 0) {
        localOffset = chapter.text.indexOf(entry.title, 0, Qt::CaseInsensitive);
    }
    return chapter.offset + qMax(qsizetype(0), localOffset);
}

QVector<ChapterOffset> epubChapterOffsets(const QVector<EpubChapterContent> &chapters,
                                          const QVector<EpubTocEntry> &tocEntries)
{
    QVector<ChapterOffset> offsets;
    for (const EpubChapterContent &chapter : chapters) {
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
        return trDocument("Password-protected PDFs are not supported yet.");
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

    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("HTML"),
                                            textFromHtml(decodeText(bytes.bytes)));
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

    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("Markdown"),
                                            textFromMarkdown(decodeText(bytes.bytes)));
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
    QStringList blocks;
    QStringList chapterTitles;
    for (const QDomElement &body : elementsByName(document, QStringLiteral("body"))) {
        collectFb2Blocks(body, &blocks);
        for (const QDomElement &section : elementsByName(body, QStringLiteral("section"))) {
            const QDomElement sectionTitle = directChildElementByName(
                section, QStringLiteral("title"));
            const QString chapterTitle = normalizedBlock(xmlNodeText(sectionTitle));
            if (!chapterTitle.isEmpty()) {
                chapterTitles.append(chapterTitle);
            }
        }
    }

    const QString text = blocks.join(QStringLiteral("\n\n")).trimmed();

    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("FB2"),
                                            text,
                                            normalizedBlock(titleElement.text()),
                                            fb2Author(titleInfo),
                                            chaptersFromTitles(text, chapterTitles));
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
    const QByteArray containerXml = zipFileData(zip, QStringLiteral("META-INF/container.xml"));
    if (!containerXml.isEmpty()) {
        QDomDocument container;
        if (container.setContent(containerXml)) {
            const QDomElement rootfile = firstElementByName(container, QStringLiteral("rootfile"));
            opfPath = cleanZipPath(QUrl::fromPercentEncoding(
                attributeByName(rootfile, QStringLiteral("full-path")).toUtf8()));
        }
    }

    if (opfPath.isEmpty()) {
        opfPath = firstZipFileWithSuffix(zip, QStringLiteral(".opf"));
    }

    if (opfPath.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not find EPUB package file."));
    }

    const QByteArray opfBytes = zipFileData(zip, opfPath);
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

    const QString basePath = zipDirectory(opfPath);
    QStringList chapterPaths;
    const QDomElement spine = firstElementByName(opf, QStringLiteral("spine"));
    for (const QDomElement &itemref : elementsByName(spine, QStringLiteral("itemref"))) {
        if (attributeByName(itemref, QStringLiteral("linear")).compare(
                QStringLiteral("no"), Qt::CaseInsensitive) == 0) {
            continue;
        }

        const EpubManifestItem item = manifest.value(
            attributeByName(itemref, QStringLiteral("idref")));
        const QString chapterPath = packageItemPath(basePath, item.href);
        if (!chapterPath.isEmpty() && !chapterPaths.contains(chapterPath)) {
            chapterPaths.append(chapterPath);
        }
    }

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

    QString text;
    QVector<EpubChapterContent> chapters;
    int chapterNumber = 1;
    for (const QString &chapterPath : chapterPaths) {
        const QByteArray chapterBytes = zipFileData(zip, chapterPath);
        if (chapterBytes.isEmpty()) {
            continue;
        }

        const QString chapterText = textFromHtml(decodeText(chapterBytes));
        if (!chapterText.isEmpty()) {
            if (!text.isEmpty()) {
                text += QStringLiteral("\n\n");
            }
            chapters.append({chapterPath,
                             chapterTitleFromHtml(chapterBytes,
                                                  chapterPath,
                                                  chapterNumber),
                             chapterText,
                             chapterBytes,
                             text.size()});
            text += chapterText;
            ++chapterNumber;
        }
    }

    if (text.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not extract readable text from EPUB."));
    }

    const QDomElement metadata = firstElementByName(opf, QStringLiteral("metadata"));
    const QDomElement titleElement = firstElementByName(metadata, QStringLiteral("title"));
    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("EPUB"),
                                            text,
                                            normalizedBlock(titleElement.text()),
                                            joinedElementValues(metadata,
                                                                QStringLiteral("creator")),
                                            normalizedChapters(
                                                text,
                                                epubChapterOffsets(chapters, tocEntries)));
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

    const QByteArray documentXml = zipFileData(zip, QStringLiteral("word/document.xml"));
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

    QStringList paragraphs;
    for (const QDomElement &paragraph : elementsByName(document, QStringLiteral("p"))) {
        QString text;
        for (const QDomElement &textRun : elementsByName(paragraph, QStringLiteral("t"))) {
            text += textRun.text();
        }

        text = normalizedBlock(text);
        if (!text.isEmpty()) {
            paragraphs.append(text);
        }
    }

    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("DOCX"),
                                            paragraphs.join(QStringLiteral("\n\n")).trimmed());
}

QStringList PdfDocumentReader::suffixes() const
{
    return {QStringLiteral("pdf")};
}

DocumentLoadResult PdfDocumentReader::load(const QFileInfo &fileInfo) const
{
    QPdfDocument document;
    const QPdfDocument::Error error = document.load(fileInfo.absoluteFilePath());
    const QString errorText = pdfErrorText(error);
    if (!errorText.isEmpty()) {
        return DocumentLoadResult::failure(errorText);
    }

    const QString title = document.metaData(QPdfDocument::MetaDataField::Title).toString().trimmed();
    const QString author = document.metaData(QPdfDocument::MetaDataField::Author).toString().trimmed();
    return DocumentLoadResult::pdfDocument(fileInfo, QStringLiteral("PDF"), title, author);
}
