#pragma once

#include <QDateTime>
#include <QString>
#include <QUrl>

enum class ReadingAnnotationType {
    Bookmark,
    Highlight
};

struct ReadingAnnotation {
    QString id;
    ReadingAnnotationType type = ReadingAnnotationType::Bookmark;
    qreal progress = 0;
    int page = -1;
    int start = -1;
    int length = 0;
    QString label;
    QString excerpt;
    QString note;
    QDateTime createdAt;
};

struct LibraryReadingAnnotation final
{
    QUrl sourceUrl;
    QString bookTitle;
    QString bookAuthor;
    QString formatName;
    ReadingAnnotation annotation;
};
