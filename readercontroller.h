#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>

#include <memory>

class LocalDocumentLoader;
struct DocumentChapter;
struct DocumentLoadResult;

class ReaderController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString displayText READ displayText NOTIFY displayTextChanged)
    Q_PROPERTY(bool richText READ richText NOTIFY richTextChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString author READ author NOTIFY authorChanged)
    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY sourcePathChanged)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(QUrl pdfSource READ pdfSource NOTIFY pdfSourceChanged)
    Q_PROPERTY(DocumentKind documentKind READ documentKind NOTIFY documentKindChanged)
    Q_PROPERTY(bool hasDocument READ hasDocument NOTIFY documentKindChanged)
    Q_PROPERTY(bool textMode READ textMode NOTIFY documentKindChanged)
    Q_PROPERTY(bool pdfMode READ pdfMode NOTIFY documentKindChanged)
    Q_PROPERTY(QString formatName READ formatName NOTIFY formatNameChanged)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY chaptersChanged)
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
    QString displayText() const;
    bool richText() const;
    QString title() const;
    QString author() const;
    QString sourcePath() const;
    QUrl sourceUrl() const;
    QUrl pdfSource() const;
    DocumentKind documentKind() const;
    bool hasDocument() const;
    bool textMode() const;
    bool pdfMode() const;
    QString formatName() const;
    QVariantList chapters() const;
    QString errorMessage() const;

    Q_INVOKABLE bool openFile(const QUrl &fileUrl);
    Q_INVOKABLE void clearError();

signals:
    void documentOpening();
    void documentOpened(const QUrl &sourceUrl);
    void textChanged();
    void displayTextChanged();
    void richTextChanged();
    void titleChanged();
    void authorChanged();
    void sourcePathChanged();
    void sourceUrlChanged();
    void pdfSourceChanged();
    void documentKindChanged();
    void formatNameChanged();
    void chaptersChanged();
    void errorMessageChanged();

private:
    void applyDocument(const DocumentLoadResult &document);
    void setText(const QString &text);
    void setDisplayText(const QString &displayText);
    void setRichText(bool richText);
    void setTitle(const QString &title);
    void setAuthor(const QString &author);
    void setSourcePath(const QString &sourcePath);
    void setSourceUrl(const QUrl &sourceUrl);
    void setPdfSource(const QUrl &pdfSource);
    void setDocumentKind(DocumentKind documentKind);
    void setFormatName(const QString &formatName);
    void setChapters(const QVector<DocumentChapter> &chapters);
    void setErrorMessage(const QString &errorMessage);

    QString m_text;
    QString m_displayText;
    bool m_richText = false;
    QString m_title;
    QString m_author;
    QString m_sourcePath;
    QUrl m_sourceUrl;
    QUrl m_pdfSource;
    DocumentKind m_documentKind = NoDocument;
    QString m_formatName;
    QVariantList m_chapters;
    QString m_errorMessage;
    std::unique_ptr<LocalDocumentLoader> m_loader;
};
