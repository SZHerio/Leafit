#pragma once

#include "richtextcontent.h"

#include <QUrl>

#include <functional>
#include <memory>

class QTextDocument;

class RichTextProcessor final
{
public:
    using ImageResolver = std::function<EmbeddedDocumentResource(const QString &source)>;
    using LinkResolver = std::function<QString(const QString &href)>;

    struct Options final
    {
        QUrl baseUrl;
        ImageResolver imageResolver;
        LinkResolver linkResolver;
        QString anchorPrefix;
        QString startAnchor;
    };

    static std::unique_ptr<QTextDocument> fromHtml(const QString &html,
                                                   const Options &options = {});
    static std::unique_ptr<QTextDocument> fromMarkdown(
        const QString &markdown,
        const Options &options = {});
    static RichTextContent content(const QTextDocument &document);
    static QString embeddedDataUrl(const EmbeddedDocumentResource &resource);

private:
    static void processDocument(QTextDocument *document, const Options &options);
};
