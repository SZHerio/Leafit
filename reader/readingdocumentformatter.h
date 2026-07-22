#pragma once

#include <QColor>
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
                                     const QString &textAlignment,
                                     const QColor &fallbackTextColor,
                                     const QColor &pageColor) const;
    Q_INVOKABLE int anchorPosition(QObject *quickTextDocument,
                                   const QString &anchor) const;
    Q_INVOKABLE void fitImages(QObject *quickTextDocument,
                               qreal maximumWidth,
                               qreal maximumHeight) const;
};
