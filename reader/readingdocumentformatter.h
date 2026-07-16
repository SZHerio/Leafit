#pragma once

#include <QObject>
#include <QString>

class ReadingDocumentFormatter final : public QObject
{
    Q_OBJECT

public:
    explicit ReadingDocumentFormatter(QObject *parent = nullptr);

    Q_INVOKABLE void applyLineHeight(QObject *quickTextDocument, qreal multiplier) const;
    Q_INVOKABLE void applyTypography(QObject *quickTextDocument,
                                     qreal lineHeight,
                                     int paragraphSpacing,
                                     int firstLineIndent,
                                     const QString &textAlignment) const;
};
