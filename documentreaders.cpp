#include "documentreaders.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QHash>
#include <QPdfDocument>
#include <QSet>
#include <QStringConverter>
#include <QStringDecoder>
#include <QTextDocument>
#include <QtCore/private/qzipreader_p.h>

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

QByteArray zipFileData(QZipReader *zip, const QString &path)
{
    QByteArray data = zip->fileData(path);
    if (!data.isEmpty()) {
        return data;
    }

    const QString decodedPath = cleanZipPath(QUrl::fromPercentEncoding(path.toUtf8()));
    if (decodedPath != path) {
        data = zip->fileData(decodedPath);
    }

    return data;
}

QString firstZipFileWithSuffix(QZipReader *zip, const QString &suffix)
{
    for (const QZipReader::FileInfo &entry : zip->fileInfoList()) {
        if (entry.isFile && entry.filePath.endsWith(suffix, Qt::CaseInsensitive)) {
            return entry.filePath;
        }
    }

    return {};
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

    const QDomElement titleElement = firstElementByName(document, QStringLiteral("book-title"));
    QStringList blocks;
    for (const QDomElement &body : elementsByName(document, QStringLiteral("body"))) {
        collectFb2Blocks(body, &blocks);
    }

    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("FB2"),
                                            blocks.join(QStringLiteral("\n\n")).trimmed(),
                                            normalizedBlock(titleElement.text()));
}

QStringList EpubDocumentReader::suffixes() const
{
    return {QStringLiteral("epub")};
}

DocumentLoadResult EpubDocumentReader::load(const QFileInfo &fileInfo) const
{
    QZipReader zip(fileInfo.absoluteFilePath());
    if (!zip.exists() || !zip.isReadable()) {
        return DocumentLoadResult::failure(trDocument("Could not open EPUB archive."));
    }

    QString opfPath;
    const QByteArray containerXml = zipFileData(&zip, QStringLiteral("META-INF/container.xml"));
    if (!containerXml.isEmpty()) {
        QDomDocument container;
        if (container.setContent(containerXml)) {
            const QDomElement rootfile = firstElementByName(container, QStringLiteral("rootfile"));
            opfPath = rootfile.attribute(QStringLiteral("full-path"));
        }
    }

    if (opfPath.isEmpty()) {
        opfPath = firstZipFileWithSuffix(&zip, QStringLiteral(".opf"));
    }

    if (opfPath.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not find EPUB package file."));
    }

    const QByteArray opfBytes = zipFileData(&zip, opfPath);
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

    struct ManifestItem
    {
        QString href;
        QString mediaType;
    };

    QHash<QString, ManifestItem> manifest;
    for (const QDomElement &item : elementsByName(opf, QStringLiteral("item"))) {
        manifest.insert(item.attribute(QStringLiteral("id")),
                        {item.attribute(QStringLiteral("href")),
                         item.attribute(QStringLiteral("media-type"))});
    }

    const QString basePath = zipDirectory(opfPath);
    QStringList chapterPaths;
    for (const QDomElement &itemref : elementsByName(opf, QStringLiteral("itemref"))) {
        const ManifestItem item = manifest.value(itemref.attribute(QStringLiteral("idref")));
        if (!item.href.isEmpty()) {
            chapterPaths.append(resolveZipPath(basePath, item.href));
        }
    }

    if (chapterPaths.isEmpty()) {
        for (const ManifestItem &item : manifest) {
            if (item.mediaType == QLatin1String("application/xhtml+xml")
                || item.mediaType == QLatin1String("text/html")) {
                chapterPaths.append(resolveZipPath(basePath, item.href));
            }
        }
    }

    QStringList chapters;
    for (const QString &chapterPath : chapterPaths) {
        const QByteArray chapterBytes = zipFileData(&zip, chapterPath);
        if (chapterBytes.isEmpty()) {
            continue;
        }

        const QString chapter = textFromHtml(decodeText(chapterBytes));
        if (!chapter.isEmpty()) {
            chapters.append(chapter);
        }
    }

    if (chapters.isEmpty()) {
        return DocumentLoadResult::failure(trDocument("Could not extract readable text from EPUB."));
    }

    const QDomElement titleElement = firstElementByName(opf, QStringLiteral("title"));
    return DocumentLoadResult::textDocument(fileInfo,
                                            QStringLiteral("EPUB"),
                                            chapters.join(QStringLiteral("\n\n")).trimmed(),
                                            normalizedBlock(titleElement.text()));
}

QStringList DocxDocumentReader::suffixes() const
{
    return {QStringLiteral("docx")};
}

DocumentLoadResult DocxDocumentReader::load(const QFileInfo &fileInfo) const
{
    QZipReader zip(fileInfo.absoluteFilePath());
    if (!zip.exists() || !zip.isReadable()) {
        return DocumentLoadResult::failure(trDocument("Could not open DOCX archive."));
    }

    const QByteArray documentXml = zipFileData(&zip, QStringLiteral("word/document.xml"));
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
    return DocumentLoadResult::pdfDocument(fileInfo, QStringLiteral("PDF"), title);
}
