#pragma once

#include "richtextcontent.h"

#include <QByteArray>
#include <QStringList>
#include <QVector>

class ZipArchiveReader;

struct EpubRenderedChapter final
{
    QString path;
    QString title;
    QString plainText;
    QByteArray markup;
    qsizetype offset = 0;
};

struct EpubRenderedContent final
{
    RichTextContent richText;
    QVector<EpubRenderedChapter> chapters;
};

class EpubContentRenderer final
{
public:
    static EpubRenderedContent render(const ZipArchiveReader &zip,
                                      const QStringList &chapterPaths);
};
