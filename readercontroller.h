#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

class LocalDocumentLoader;
struct DocumentLoadResult;

class ReaderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(QUrl pdfSource READ pdfSource NOTIFY pdfSourceChanged)
    Q_PROPERTY(DocumentKind documentKind READ documentKind NOTIFY documentKindChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentKindChanged)
    Q_PROPERTY(bool textMode READ textMode NOTIFY documentKindChanged)
    Q_PROPERTY(bool pdfMode READ pdfMode NOTIFY documentKindChanged)
    Q_PROPERTY(QString formatName READ formatName NOTIFY formatNameChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)

public:
    enum DocumentKind {
        NoDocument,
        TextDocument,
        PdfDocument
    };
    Q_ENUM(DocumentKind)

    explicit ReaderController(QObject *parent = nullptr);
    ~ReaderController() override;

    QString text() const;
    QString title() const;
    QString sourcePath() const;
    QUrl sourceUrl() const;
    QUrl pdfSource() const;
    DocumentKind documentKind() const;
    bool hasDocument() const;
    bool textMode() const;
    bool pdfMode() const;
    QString formatName() const;
    QString errorMessage() const;

    Q_INVOKABLE bool openFile(const QUrl &fileUrl);
    Q_INVOKABLE void clearError();

signals:
    void documentOpening();
    void documentOpened(const QUrl &sourceUrl);
    void textChanged();
    void titleChanged();
    void sourcePathChanged();
    void sourceUrlChanged();
    void pdfSourceChanged();
    void documentKindChanged();
    void formatNameChanged();
    void errorMessageChanged();

private:
    void applyDocument(const DocumentLoadResult &document);
    void setText(const QString &text);
    void setTitle(const QString &title);
    void setSourcePath(const QString &sourcePath);
    void setSourceUrl(const QUrl &sourceUrl);
    void setPdfSource(const QUrl &pdfSource);
    void setDocumentKind(DocumentKind documentKind);
    void setFormatName(const QString &formatName);
    void setErrorMessage(const QString &errorMessage);

    QString m_text;
    QString m_title;
    QString m_sourcePath;
    QUrl m_sourceUrl;
    QUrl m_pdfSource;
    DocumentKind m_documentKind = NoDocument;
    QString m_formatName;
    QString m_errorMessage;
    std::unique_ptr<LocalDocumentLoader> m_loader;
};
