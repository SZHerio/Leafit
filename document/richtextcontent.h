#pragma once

#include <QByteArray>
#include <QString>

struct EmbeddedDocumentResource final
{
    QByteArray data;
    QString mimeType;

    bool isValid() const
    {
        return !data.isEmpty() && mimeType.startsWith(QStringLiteral("image/"));
    }
};

struct RichTextContent final
{
    QString plainText;
    QString html;
    QString title;

    bool isEmpty() const
    {
        return plainText.trimmed().isEmpty();
    }
};
