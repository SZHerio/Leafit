#include "richtextprocessor.h"

#include <QFile>
#include <QMimeDatabase>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QVector>

#include <algorithm>

namespace {

constexpr qsizetype maximumEmbeddedImageSize = 16 * 1024 * 1024;

struct FormattedRange final
{
    int position = 0;
    int length = 0;
    QTextCharFormat format;
};

void isolateBoundaryImages(QTextDocument *document)
{
    QVector<int> splitPositions;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        const QString text = block.text();
        bool hasText = false;
        for (const QChar character : text) {
            if (!character.isSpace()
                && character != QChar::ObjectReplacementCharacter) {
                hasText = true;
                break;
            }
        }
        if (!hasText) {
            continue;
        }

        qsizetype first = 0;
        while (first < text.size() && text.at(first).isSpace()) {
            ++first;
        }
        if (first < text.size()
            && text.at(first) == QChar::ObjectReplacementCharacter) {
            qsizetype split = first;
            while (split < text.size()
                   && (text.at(split).isSpace()
                       || text.at(split) == QChar::ObjectReplacementCharacter)) {
                ++split;
            }
            if (split < text.size()) {
                splitPositions.append(block.position() + static_cast<int>(split));
            }
        }

        qsizetype last = text.size() - 1;
        while (last >= 0 && text.at(last).isSpace()) {
            --last;
        }
        if (last >= 0
            && text.at(last) == QChar::ObjectReplacementCharacter) {
            qsizetype split = last;
            while (split >= 0
                   && (text.at(split).isSpace()
                       || text.at(split) == QChar::ObjectReplacementCharacter)) {
                --split;
            }
            ++split;
            if (split > 0) {
                splitPositions.append(block.position() + static_cast<int>(split));
            }
        }
    }

    std::sort(splitPositions.begin(), splitPositions.end(), std::greater<int>());
    splitPositions.erase(std::unique(splitPositions.begin(), splitPositions.end()),
                         splitPositions.end());

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const int position : splitPositions) {
        cursor.setPosition(position);
        cursor.insertBlock();
    }
    cursor.endEditBlock();
}

EmbeddedDocumentResource localImage(const QString &source, const QUrl &baseUrl)
{
    QUrl imageUrl(source);
    if (imageUrl.isRelative()) {
        imageUrl = baseUrl.resolved(imageUrl);
    }
    if (!imageUrl.isLocalFile()) {
        return {};
    }

    QFile imageFile(imageUrl.toLocalFile());
    if (!imageFile.open(QIODevice::ReadOnly)
        || imageFile.size() <= 0
        || imageFile.size() > maximumEmbeddedImageSize) {
        return {};
    }

    const QByteArray data = imageFile.readAll();
    const QString mimeType = QMimeDatabase().mimeTypeForData(data).name();
    return {data, mimeType};
}

QString defaultLink(const QString &href, const QUrl &baseUrl)
{
    const QString trimmedHref = href.trimmed();
    if (trimmedHref.isEmpty() || trimmedHref.startsWith(u'#')) {
        return trimmedHref;
    }

    QUrl linkUrl(trimmedHref);
    if (linkUrl.isRelative()) {
        linkUrl = baseUrl.resolved(linkUrl);
    }
    return linkUrl.toString(QUrl::FullyEncoded);
}

} // namespace

std::unique_ptr<QTextDocument> RichTextProcessor::fromHtml(
    const QString &html,
    const Options &options)
{
    auto document = std::make_unique<QTextDocument>();
    document->setBaseUrl(options.baseUrl);
    document->setHtml(html);
    processDocument(document.get(), options);
    return document;
}

std::unique_ptr<QTextDocument> RichTextProcessor::fromMarkdown(
    const QString &markdown,
    const Options &options)
{
    auto document = std::make_unique<QTextDocument>();
    document->setBaseUrl(options.baseUrl);
    document->setMarkdown(markdown);
    processDocument(document.get(), options);
    return document;
}

RichTextContent RichTextProcessor::content(const QTextDocument &document)
{
    return {
        document.toPlainText(),
        document.toHtml(),
        document.metaInformation(QTextDocument::DocumentTitle).trimmed()
    };
}

QString RichTextProcessor::embeddedDataUrl(const EmbeddedDocumentResource &resource)
{
    if (!resource.isValid()) {
        return {};
    }

    return QStringLiteral("data:%1;base64,%2")
        .arg(resource.mimeType,
             QString::fromLatin1(resource.data.toBase64()));
}

void RichTextProcessor::processDocument(QTextDocument *document,
                                        const Options &options)
{
    if (!document) {
        return;
    }

    isolateBoundaryImages(document);

    QVector<FormattedRange> ranges;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()) {
                ranges.append({fragment.position(),
                               fragment.length(),
                               fragment.charFormat()});
            }
        }
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const FormattedRange &range : ranges) {
        QTextCharFormat format = range.format;
        bool changed = false;

        if (format.isImageFormat()) {
            QTextImageFormat imageFormat = format.toImageFormat();
            const QString source = imageFormat.name().trimmed();
            if (!source.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
                const EmbeddedDocumentResource resource = options.imageResolver
                    ? options.imageResolver(source)
                    : localImage(source, options.baseUrl);
                const QString dataUrl = embeddedDataUrl(resource);
                if (!dataUrl.isEmpty()) {
                    imageFormat.setName(dataUrl);
                } else {
                    imageFormat.setName(QString());
                    imageFormat.setWidth(0);
                    imageFormat.setHeight(0);
                }
                format = imageFormat;
                changed = true;
            }
        }

        QStringList anchorNames = format.anchorNames();
        if (!options.anchorPrefix.isEmpty() && !anchorNames.isEmpty()) {
            for (QString &anchorName : anchorNames) {
                anchorName.prepend(options.anchorPrefix);
            }
            format.setAnchorNames(anchorNames);
            changed = true;
        }

        if (format.isAnchor() && !format.anchorHref().isEmpty()) {
            const QString resolvedHref = options.linkResolver
                ? options.linkResolver(format.anchorHref())
                : defaultLink(format.anchorHref(), options.baseUrl);
            if (resolvedHref != format.anchorHref()) {
                format.setAnchorHref(resolvedHref);
                changed = true;
            }
        }

        if (changed) {
            cursor.setPosition(range.position);
            cursor.setPosition(range.position + range.length,
                               QTextCursor::KeepAnchor);
            cursor.setCharFormat(format);
        }
    }

    if (!options.startAnchor.isEmpty() && document->characterCount() > 1) {
        cursor.setPosition(0);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
        QTextCharFormat format = cursor.charFormat();
        QStringList anchorNames = format.anchorNames();
        if (!anchorNames.contains(options.startAnchor)) {
            anchorNames.prepend(options.startAnchor);
            format.setAnchorNames(anchorNames);
            cursor.setCharFormat(format);
        }
    }
    cursor.endEditBlock();
}
