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
#include <QTextTable>
#include <QTextTableFormat>
#include <QSet>

#include <cmath>

namespace {

constexpr int originalImageWidthProperty = QTextFormat::UserProperty + 101;
constexpr int originalImageHeightProperty = QTextFormat::UserProperty + 102;
constexpr int codeBlockProperty = QTextFormat::UserProperty + 103;
constexpr int generatedCodeBackgroundProperty = QTextFormat::UserProperty + 104;
constexpr int generatedInlineCodeProperty = QTextFormat::UserProperty + 105;
constexpr int generatedTableStyleProperty = QTextFormat::UserProperty + 106;

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

QColor blendedColor(const QColor &background, const QColor &foreground, qreal amount)
{
    const qreal boundedAmount = qBound(qreal(0), amount, qreal(1));
    return QColor::fromRgbF(background.redF() * (1 - boundedAmount)
                                + foreground.redF() * boundedAmount,
                            background.greenF() * (1 - boundedAmount)
                                + foreground.greenF() * boundedAmount,
                            background.blueF() * (1 - boundedAmount)
                                + foreground.blueF() * boundedAmount,
                            1);
}

QColor readableColor(const QColor &preferred, const QColor &background)
{
    if (contrastRatio(preferred, background) >= 4.5) {
        return preferred;
    }
    const QColor black(Qt::black);
    const QColor white(Qt::white);
    return contrastRatio(black, background) >= contrastRatio(white, background)
               ? black
               : white;
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

bool fixedPitchFormat(const QTextCharFormat &format)
{
    if (format.fontFixedPitch() || format.font().styleHint() == QFont::Monospace) {
        return true;
    }
    for (const QString &family : format.font().families()) {
        const QString normalized = family.toLower();
        if (normalized.contains(QStringLiteral("mono"))
            || normalized.contains(QStringLiteral("courier"))
            || normalized.contains(QStringLiteral("consolas"))) {
            return true;
        }
    }
    return false;
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
    const QColor codeBackground = blendedColor(pageColor, fallbackTextColor, 0.07);
    const QColor tableBorderColor = blendedColor(pageColor, fallbackTextColor, 0.28);
    QSet<QTextTable *> tables;

    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(boundedLineHeight * 100,
                             QTextBlockFormat::ProportionalHeight);

        bool hasImage = false;
        bool hasVisibleText = false;
        bool hasFixedPitchText = false;
        bool hasProportionalText = false;
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            hasImage = hasImage || fragment.charFormat().isImageFormat();
            hasVisibleText = hasVisibleText
                             || (!fragment.charFormat().isImageFormat()
                                 && !fragment.text().trimmed().isEmpty());
            if (!fragment.charFormat().isImageFormat()
                && !fragment.text().trimmed().isEmpty()) {
                if (fixedPitchFormat(fragment.charFormat())) {
                    hasFixedPitchText = true;
                } else {
                    hasProportionalText = true;
                }
            }
        }

        QTextCursor blockCursor(block);
        QTextTable *table = blockCursor.currentTable();
        if (table) {
            tables.insert(table);
        }
        const bool codeBlock = format.nonBreakableLines()
                               || format.property(codeBlockProperty).toBool()
                               || (hasFixedPitchText && !hasProportionalText);
        const bool imageBlock = hasImage && !hasVisibleText;
        const bool structuralBlock = block.textList()
                                     || format.headingLevel() > 0
                                     || table
                                     || codeBlock
                                     || imageBlock;
        if (!structuralBlock) {
            format.setBottomMargin(boundedSpacing);
            format.setTextIndent(boundedIndent);
            format.setAlignment(alignment);
        } else if (table) {
            format.setTextIndent(0);
            format.setAlignment(Qt::AlignLeft);
            format.setBottomMargin(0);
        } else if (codeBlock) {
            format.setNonBreakableLines(false);
            format.setProperty(codeBlockProperty, true);
            format.setTextIndent(0);
            format.setAlignment(Qt::AlignLeft);
            format.setTopMargin(qMax(qreal(6), format.topMargin()));
            format.setBottomMargin(qMax(qreal(6), format.bottomMargin()));
            if (format.background().style() == Qt::NoBrush
                || format.property(generatedCodeBackgroundProperty).toBool()) {
                format.setBackground(codeBackground);
                format.setProperty(generatedCodeBackgroundProperty, true);
            }
        } else if (imageBlock) {
            format.setTextIndent(0);
            format.setAlignment(Qt::AlignHCenter);
            format.setBottomMargin(qMax(boundedSpacing, 8));
        }

        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }

    for (QTextTable *table : tables) {
        QTextTableFormat format = table->format();
        const bool generatedStyle = format.property(generatedTableStyleProperty).toBool();
        if (format.border() <= 0 || generatedStyle) {
            format.setBorder(1);
            format.setBorderBrush(tableBorderColor);
            format.setBorderStyle(QTextFrameFormat::BorderStyle_Solid);
            format.setProperty(generatedTableStyleProperty, true);
        }
        format.setCellSpacing(0);
        format.setCellPadding(qMax(qreal(6), format.cellPadding()));
        format.setWidth(QTextLength(QTextLength::PercentageLength, 100));
        table->setFormat(format);
    }

    for (const TextRange &textRange : textRanges) {
        const bool link = textRange.format.isAnchor()
                          && !textRange.format.anchorHref().isEmpty();
        const QBrush foreground = textRange.format.foreground();
        const bool hasBackground = textRange.format.background().style()
                                   != Qt::NoBrush;
        const bool inlineCode = fixedPitchFormat(textRange.format);
        const bool generatedInlineBackground = textRange.format.property(
            generatedInlineCodeProperty).toBool();
        const QColor effectiveBackground = inlineCode
                                                   && (!hasBackground
                                                       || generatedInlineBackground)
                                               ? codeBackground
                                               : hasBackground
                                                 ? textRange.format.background().color()
                                                 : pageColor;
        const bool lowContrast = foreground.style() == Qt::NoBrush
                                 || contrastRatio(foreground.color(),
                                                  effectiveBackground) < 4.5;
        if (!link && !inlineCode && !lowContrast) {
            continue;
        }

        QTextCharFormat readableFormat;
        if (lowContrast || link) {
            readableFormat.setForeground(readableColor(fallbackTextColor,
                                                        effectiveBackground));
        }
        if (link) {
            readableFormat.setFontUnderline(true);
        }
        if (inlineCode
            && (!hasBackground || generatedInlineBackground)) {
            readableFormat.setBackground(codeBackground);
            readableFormat.setProperty(generatedInlineCodeProperty, true);
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
                                         qreal maximumWidth,
                                         qreal maximumHeight) const
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

            qreal scale = qMin(qreal(1), qMax(qreal(1), maximumWidth) / originalWidth);
            if (maximumHeight > 0) {
                scale = qMin(scale, maximumHeight / originalHeight);
            }
            format.setWidth(qMax(qreal(1), originalWidth * scale));
            format.setHeight(qMax(qreal(1), originalHeight * scale));
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
