#include "readingdocumentformatter.h"

#include <QQuickTextDocument>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

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
