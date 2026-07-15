#pragma once

#include <QDateTime>
#include <QString>
#include <QUrl>

struct LibraryBook final
{
    QUrl sourceUrl;
    QString sourcePath;
    QString title;
    QString formatName;
    qreal progress = 0;
    QDateTime lastOpened;
    bool fileAvailable = false;
};
