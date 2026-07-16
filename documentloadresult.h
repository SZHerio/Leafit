#pragma once

#include <QFileInfo>
#include <QString>
#include <QUrl>
#include <QVector>

enum class DocumentViewMode {
    None,
    Text,
    Pdf
};

struct DocumentChapter final
{
    QString title;
    qreal progress = 0;
};

struct DocumentLoadResult
{
    DocumentViewMode viewMode = DocumentViewMode::None;
    QString text;
    QString displayText;
    bool richText = false;
    QString title;
    QString author;
    QString sourcePath;
    QUrl sourceUrl;
    QUrl pdfSource;
    QString formatName;
    QVector<DocumentChapter> chapters;
    QString errorMessage;

    bool isSuccess() const
    {
        return viewMode != DocumentViewMode::None && errorMessage.isEmpty();
    }

    static DocumentLoadResult failure(const QString &errorMessage)
    {
        DocumentLoadResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    static DocumentLoadResult textDocument(const QFileInfo &fileInfo,
                                           const QString &formatName,
                                           const QString &text,
                                           const QString &title = {},
                                           const QString &author = {},
                                           const QVector<DocumentChapter> &chapters = {})
    {
        DocumentLoadResult result;
        result.viewMode = DocumentViewMode::Text;
        result.text = text;
        result.displayText = text;
        result.title = title.isEmpty() ? fileInfo.completeBaseName() : title;
        result.author = author;
        result.sourcePath = fileInfo.absoluteFilePath();
        result.sourceUrl = QUrl::fromLocalFile(result.sourcePath);
        result.formatName = formatName;
        result.chapters = chapters;
        return result;
    }

    static DocumentLoadResult richTextDocument(
        const QFileInfo &fileInfo,
        const QString &formatName,
        const QString &text,
        const QString &displayText,
        const QString &title = {},
        const QString &author = {},
        const QVector<DocumentChapter> &chapters = {})
    {
        DocumentLoadResult result = textDocument(fileInfo,
                                                 formatName,
                                                 text,
                                                 title,
                                                 author,
                                                 chapters);
        result.displayText = displayText;
        result.richText = true;
        return result;
    }

    static DocumentLoadResult pdfDocument(const QFileInfo &fileInfo,
                                          const QString &formatName,
                                          const QString &title = {},
                                          const QString &author = {})
    {
        DocumentLoadResult result;
        result.viewMode = DocumentViewMode::Pdf;
        result.title = title.isEmpty() ? fileInfo.completeBaseName() : title;
        result.author = author;
        result.sourcePath = fileInfo.absoluteFilePath();
        result.sourceUrl = QUrl::fromLocalFile(result.sourcePath);
        result.pdfSource = result.sourceUrl;
        result.formatName = formatName;
        return result;
    }
};
