#include "readercontroller.h"

#include "documentloadresult.h"
#include "localdocumentloader.h"

#include <QVariantMap>

#include <memory>

namespace {

ReaderController::DocumentKind toControllerKind(DocumentViewMode viewMode)
{
    switch (viewMode) {
    case DocumentViewMode::Text:
        return ReaderController::TextDocument;
    case DocumentViewMode::Pdf:
        return ReaderController::PdfDocument;
    case DocumentViewMode::None:
        return ReaderController::NoDocument;
    }

    return ReaderController::NoDocument;
}

} // namespace

ReaderController::ReaderController(QObject *parent)
    : QObject(parent)
    , m_loader(std::make_unique<LocalDocumentLoader>())
{
}

ReaderController::~ReaderController() = default;

QString ReaderController::text() const
{
    return m_text;
}

QString ReaderController::displayText() const
{
    return m_displayText;
}

bool ReaderController::richText() const
{
    return m_richText;
}

QString ReaderController::title() const
{
    return m_title;
}

QString ReaderController::author() const
{
    return m_author;
}

QString ReaderController::sourcePath() const
{
    return m_sourcePath;
}

QUrl ReaderController::sourceUrl() const
{
    return m_sourceUrl;
}

QUrl ReaderController::pdfSource() const
{
    return m_pdfSource;
}

ReaderController::DocumentKind ReaderController::documentKind() const
{
    return m_documentKind;
}

bool ReaderController::hasDocument() const
{
    return m_documentKind != NoDocument;
}

bool ReaderController::textMode() const
{
    return m_documentKind == TextDocument;
}

bool ReaderController::pdfMode() const
{
    return m_documentKind == PdfDocument;
}

QString ReaderController::formatName() const
{
    return m_formatName;
}

QVariantList ReaderController::chapters() const
{
    return m_chapters;
}

QString ReaderController::errorMessage() const
{
    return m_errorMessage;
}

bool ReaderController::openFile(const QUrl &fileUrl)
{
    emit documentOpening();
    clearError();

    const DocumentLoadResult document = m_loader->load(fileUrl);
    if (!document.isSuccess()) {
        setErrorMessage(document.errorMessage);
        return false;
    }

    applyDocument(document);
    emit documentOpened(m_sourceUrl);
    return true;
}

void ReaderController::clearError()
{
    setErrorMessage(QString());
}

void ReaderController::applyDocument(const DocumentLoadResult &document)
{
    setText(document.text);
    setDisplayText(document.displayText);
    setRichText(document.richText);
    setTitle(document.title);
    setAuthor(document.author);
    setSourcePath(document.sourcePath);
    setSourceUrl(document.sourceUrl);
    setPdfSource(document.pdfSource);
    setDocumentKind(toControllerKind(document.viewMode));
    setFormatName(document.formatName);
    setChapters(document.chapters);
}

void ReaderController::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    emit textChanged();
}

void ReaderController::setDisplayText(const QString &displayText)
{
    if (m_displayText == displayText) {
        return;
    }

    m_displayText = displayText;
    emit displayTextChanged();
}

void ReaderController::setRichText(bool richText)
{
    if (m_richText == richText) {
        return;
    }

    m_richText = richText;
    emit richTextChanged();
}

void ReaderController::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }

    m_title = title;
    emit titleChanged();
}

void ReaderController::setAuthor(const QString &author)
{
    if (m_author == author) {
        return;
    }

    m_author = author;
    emit authorChanged();
}

void ReaderController::setSourcePath(const QString &sourcePath)
{
    if (m_sourcePath == sourcePath) {
        return;
    }

    m_sourcePath = sourcePath;
    emit sourcePathChanged();
}

void ReaderController::setSourceUrl(const QUrl &sourceUrl)
{
    if (m_sourceUrl == sourceUrl) {
        return;
    }

    m_sourceUrl = sourceUrl;
    emit sourceUrlChanged();
}

void ReaderController::setPdfSource(const QUrl &pdfSource)
{
    if (m_pdfSource == pdfSource) {
        return;
    }

    m_pdfSource = pdfSource;
    emit pdfSourceChanged();
}

void ReaderController::setDocumentKind(DocumentKind documentKind)
{
    if (m_documentKind == documentKind) {
        return;
    }

    m_documentKind = documentKind;
    emit documentKindChanged();
}

void ReaderController::setFormatName(const QString &formatName)
{
    if (m_formatName == formatName) {
        return;
    }

    m_formatName = formatName;
    emit formatNameChanged();
}

void ReaderController::setChapters(const QVector<DocumentChapter> &chapters)
{
    QVariantList chapterList;
    chapterList.reserve(chapters.size());
    for (const DocumentChapter &chapter : chapters) {
        chapterList.append(QVariantMap{
            {QStringLiteral("title"), chapter.title},
            {QStringLiteral("progress"), chapter.progress}
        });
    }

    if (m_chapters == chapterList) {
        return;
    }

    m_chapters = chapterList;
    emit chaptersChanged();
}

void ReaderController::setErrorMessage(const QString &errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }

    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}
