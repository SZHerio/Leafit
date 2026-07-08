#include "readercontroller.h"

#include "documentloadresult.h"
#include "localdocumentloader.h"

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

QString ReaderController::title() const
{
    return m_title;
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

bool ReaderController::pdfMode() const
{
    return m_documentKind == PdfDocument;
}

QString ReaderController::formatName() const
{
    return m_formatName;
}

QString ReaderController::errorMessage() const
{
    return m_errorMessage;
}

bool ReaderController::openFile(const QUrl &fileUrl)
{
    clearError();

    const DocumentLoadResult document = m_loader->load(fileUrl);
    if (!document.isSuccess()) {
        setErrorMessage(document.errorMessage);
        return false;
    }

    applyDocument(document);
    return true;
}

void ReaderController::clearError()
{
    setErrorMessage(QString());
}

void ReaderController::applyDocument(const DocumentLoadResult &document)
{
    setText(document.text);
    setTitle(document.title);
    setSourcePath(document.sourcePath);
    setSourceUrl(document.sourceUrl);
    setPdfSource(document.pdfSource);
    setDocumentKind(toControllerKind(document.viewMode));
    setFormatName(document.formatName);
}

void ReaderController::setText(const QString &text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    emit textChanged();
}

void ReaderController::setTitle(const QString &title)
{
    if (m_title == title) {
        return;
    }

    m_title = title;
    emit titleChanged();
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

void ReaderController::setErrorMessage(const QString &errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }

    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}
