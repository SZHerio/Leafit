#pragma once

#include <QObject>

class ReadingDocumentFormatter final : public QObject
{
    Q_OBJECT

public:
    explicit ReadingDocumentFormatter(QObject *parent = nullptr);

    Q_INVOKABLE void applyLineHeight(QObject *quickTextDocument, qreal multiplier) const;
};
