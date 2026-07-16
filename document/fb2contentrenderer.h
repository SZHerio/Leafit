#pragma once

#include "richtextcontent.h"

#include <QStringList>

class QDomDocument;

struct Fb2RenderedContent final
{
    RichTextContent richText;
    QStringList chapterTitles;
};

class Fb2ContentRenderer final
{
public:
    static Fb2RenderedContent render(const QDomDocument &document);
};
