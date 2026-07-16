#include "readingdocumentformatter.h"

#include <QQuickTextDocument>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextList>

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
                                                const QString &textAlignment) const
{
    auto *wrapper = qobject_cast<QQuickTextDocument *>(quickTextDocument);
    if (!wrapper || !wrapper->textDocument()) {
        return;
    }

    QTextDocument *document = wrapper->textDocument();
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

    cursor.endEditBlock();
}
