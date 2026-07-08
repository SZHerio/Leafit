#pragma once

#include <QFileInfo>
#include <QString>
#include <QUrl>

enum class DocumentViewMode {
    None,
    Text,
    Pdf
};

struct DocumentLoadResult
{
    DocumentViewMode viewMode = DocumentViewMode::None;
    QString text;
    QString title;
    QString sourcePath;
    QUrl sourceUrl;
    QUrl pdfSource;
    QString formatName;
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
                                           const QString &title = {})
    {
        DocumentLoadResult result;
        result.viewMode = DocumentViewMode::Text;
        result.text = text;
        result.title = title.isEmpty() ? fileInfo.completeBaseName() : title;
        result.sourcePath = fileInfo.absoluteFilePath();
        result.sourceUrl = QUrl::fromLocalFile(result.sourcePath);
        result.formatName = formatName;
        return result;
    }

    static DocumentLoadResult pdfDocument(const QFileInfo &fileInfo,
                                          const QString &formatName,
                                          const QString &title = {})
    {
        DocumentLoadResult result;
        result.viewMode = DocumentViewMode::Pdf;
        result.title = title.isEmpty() ? fileInfo.completeBaseName() : title;
        result.sourcePath = fileInfo.absoluteFilePath();
        result.sourceUrl = QUrl::fromLocalFile(result.sourcePath);
        result.pdfSource = result.sourceUrl;
        result.formatName = formatName;
        return result;
    }
};
