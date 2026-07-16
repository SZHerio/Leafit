#include "readingdocumentformatter.h"

#include <QQuickTextDocument>
#include <QImage>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextImageFormat>

#include <cmath>

namespace {

constexpr int originalImageWidthProperty = QTextFormat::UserProperty + 101;
constexpr int originalImageHeightProperty = QTextFormat::UserProperty + 102;

struct ImageRange final
{
    int position = 0;
    int length = 0;
    QTextImageFormat format;
};

struct TextRange final
{
    int position = 0;
    int length = 0;
    QTextCharFormat format;
};

qreal linearColorChannel(qreal channel)
{
    return channel <= 0.04045
               ? channel / 12.92
               : std::pow((channel + 0.055) / 1.055, 2.4);
}

qreal relativeLuminance(const QColor &color)
{
    return 0.2126 * linearColorChannel(color.redF())
           + 0.7152 * linearColorChannel(color.greenF())
           + 0.0722 * linearColorChannel(color.blueF());
}

qreal contrastRatio(const QColor &first, const QColor &second)
{
    const qreal firstLuminance = relativeLuminance(first);
    const qreal secondLuminance = relativeLuminance(second);
    const qreal lighter = qMax(firstLuminance, secondLuminance);
    const qreal darker = qMin(firstLuminance, secondLuminance);
    return (lighter + 0.05) / (darker + 0.05);
}

QSizeF embeddedImageSize(const QString &source)
{
    if (!source.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        return {};
    }

    const qsizetype separator = source.indexOf(u',');
    if (separator < 0) {
        return {};
    }

    const QByteArray bytes = QByteArray::fromBase64(
        source.mid(separator + 1).toLatin1());
    QImage image;
    if (!image.loadFromData(bytes)) {
        return {};
    }
    return image.size();
}

} // namespace

ReadingDocumentFormatter::ReadingDocumentFormatter(QObject *parent)
    : QObject(parent)
{
}

void ReadingDocumentFormatter::applyLineHeight(QObject *quickTextDocument, qreal multiplier) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!wrapper || !wrapper->textDocument()) {
        return;
    }

    QTextCursor cursor(wrapper->textDocument());
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);

    QTextBlockFormat format;
    format.setLineHeight(qBound(qreal(1.0), multiplier, qreal(2.5)) * 100,
                         QTextBlockFormat::ProportionalHeight);
    cursor.mergeBlockFormat(format);
    cursor.endEditBlock();
}

void ReadingDocumentFormatter::applyTypography(QObject *quickTextDocument,
                                                qreal lineHeight,
                                                int paragraphSpacing,
                                                int firstLineIndent,
                                                const QString &textAlignment,
                                                const QColor &fallbackTextColor,
                                                const QColor &pageColor) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!wrapper || !wrapper->textDocument()) {
        return;
    }

    QTextDocument *document = wrapper->textDocument();
    QVector<TextRange> textRanges;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()
                && !fragment.charFormat().isImageFormat()) {
                textRanges.append({fragment.position(),
                                   fragment.length(),
                                   fragment.charFormat()});
            }
        }
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();

    const qreal boundedLineHeight = qBound(qreal(1.0), lineHeight, qreal(2.5));
    const int boundedSpacing = qBound(0, paragraphSpacing, 64);
    const int boundedIndent = qBound(0, firstLineIndent, 96);
    const Qt::Alignment alignment = textAlignment.compare(
                                        QStringLiteral("left"),
                                        Qt::CaseInsensitive) == 0
                                        ? Qt::AlignLeft
                                        : Qt::AlignJustify;

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(boundedLineHeight * 100,
                             QTextBlockFormat::ProportionalHeight);

        const bool structuralBlock = block.textList()
                                     || format.headingLevel() > 0;
        if (!structuralBlock) {
            format.setBottomMargin(boundedSpacing);
            format.setTextIndent(boundedIndent);
            format.setAlignment(alignment);
        }

        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }

    for (const TextRange &textRange : textRanges) {
        const bool link = textRange.format.isAnchor()
                          && !textRange.format.anchorHref().isEmpty();
        const QBrush foreground = textRange.format.foreground();
        const bool hasBackground = textRange.format.background().style()
                                   != Qt::NoBrush;
        const bool lowContrast = foreground.style() == Qt::NoBrush
                                 || contrastRatio(foreground.color(), pageColor) < 4.5;
        if (!link && (hasBackground || !lowContrast)) {
            continue;
        }

        QTextCharFormat readableFormat;
        readableFormat.setForeground(fallbackTextColor);
        if (link) {
            readableFormat.setFontUnderline(true);
        }
        cursor.setPosition(textRange.position);
        cursor.setPosition(textRange.position + textRange.length,
                           QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(readableFormat);
    }

    cursor.endEditBlock();
}

int ReadingDocumentFormatter::anchorPosition(QObject *quickTextDocument,
                                             const QString &anchor) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!wrapper || !wrapper->textDocument()) {
        return -1;
    }

    QString normalizedAnchor = anchor.trimmed();
    if (normalizedAnchor.startsWith(u'#')) {
        normalizedAnchor.remove(0, 1);
    }
    if (normalizedAnchor.isEmpty()) {
        return 0;
    }

    for (QTextBlock block = wrapper->textDocument()->begin();
         block.isValid();
         block = block.next()) {
        for (QTextBlock::Iterator fragmentIterator = block.begin();
             !fragmentIterator.atEnd();
             ++fragmentIterator) {
            const QTextFragment fragment = fragmentIterator.fragment();
            if (fragment.isValid()
                && fragment.charFormat().anchorNames().contains(normalizedAnchor)) {
                return fragment.position();
            }
        }
    }

    return -1;
}

void ReadingDocumentFormatter::fitImages(QObject *quickTextDocument,
                                         qreal maximumWidth) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!wrapper || !wrapper->textDocument() || maximumWidth <= 0) {
        return;
    }

    QTextDocument *document = wrapper->textDocument();
    QVector<ImageRange> images;

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid() || !fragment.charFormat().isImageFormat()) {
                continue;
            }

            QTextImageFormat format = fragment.charFormat().toImageFormat();
            qreal originalWidth = format.property(originalImageWidthProperty).toReal();
            qreal originalHeight = format.property(originalImageHeightProperty).toReal();
            if (originalWidth <= 0 || originalHeight <= 0) {
                const QSizeF intrinsicSize = embeddedImageSize(format.name());
                originalWidth = format.width() > 0 ? format.width() : intrinsicSize.width();
                originalHeight = format.height() > 0 ? format.height() : intrinsicSize.height();
                if (originalWidth <= 0 || originalHeight <= 0) {
                    continue;
                }
                format.setProperty(originalImageWidthProperty, originalWidth);
                format.setProperty(originalImageHeightProperty, originalHeight);
            }

            const qreal fittedWidth = qMin(originalWidth, qMax(qreal(1), maximumWidth));
            format.setWidth(fittedWidth);
            format.setHeight(originalHeight * fittedWidth / originalWidth);
            images.append({fragment.position(), fragment.length(), format});
        }
    }

    QTextCursor cursor(document);
    cursor.beginEditBlock();
    for (const ImageRange &image : images) {
        cursor.setPosition(image.position);
        cursor.setPosition(image.position + image.length,
                           QTextCursor::KeepAnchor);
        cursor.setCharFormat(image.format);
    }

    cursor.endEditBlock();
}
