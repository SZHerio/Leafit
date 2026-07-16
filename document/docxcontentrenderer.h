#pragma once

#include "richtextcontent.h"

#include <QStringList>

class QDomDocument;
class ZipArchiveReader;

struct DocxRenderedContent final
{
    RichTextContent richText;
    QString title;
    QString author;
    QStringList chapterTitles;
};

class DocxContentRenderer final
{
public:
    static DocxRenderedContent render(const ZipArchiveReader &zip,
                                      const QDomDocument &document);
};
