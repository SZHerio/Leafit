#pragma once

#include "../library/librarybook.h"
#include "../library/bookmetadatapatch.h"
#include "profiledatabase.h"
#include "profilestorage.h"

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

class LocalStateStore final : public QObject, public ProfileStorage
{
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(QString colorTheme READ colorTheme WRITE setColorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    Q_PROPERTY(QString readingFont READ readingFont WRITE setReadingFont NOTIFY readingFontChanged)
    Q_PROPERTY(int textFontSize READ textFontSize WRITE setTextFontSize NOTIFY textFontSizeChanged)
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY lineHeightChanged)
    Q_PROPERTY(int paragraphSpacing READ paragraphSpacing WRITE setParagraphSpacing NOTIFY paragraphSpacingChanged)
    Q_PROPERTY(int firstLineIndent READ firstLineIndent WRITE setFirstLineIndent NOTIFY firstLineIndentChanged)
    Q_PROPERTY(QString textAlignment READ textAlignment WRITE setTextAlignment NOTIFY textAlignmentChanged)
    Q_PROPERTY(int pageWidth READ pageWidth WRITE setPageWidth NOTIFY pageWidthChanged)
    Q_PROPERTY(int scrollSpeed READ scrollSpeed WRITE setScrollSpeed NOTIFY scrollSpeedChanged)
    Q_PROPERTY(QUrl lastBookUrl READ lastBookUrl WRITE setLastBookUrl NOTIFY lastBookUrlChanged)

public:
    explicit LocalStateStore(QObject *parent = nullptr);
    explicit LocalStateStore(const QString &settingsFilePath, QObject *parent = nullptr);

    bool darkMode() const;
    QString colorTheme() const;
    QString language() const;
    QString readingFont() const;
    int textFontSize() const;
    qreal lineHeight() const;
    int paragraphSpacing() const;
    int firstLineIndent() const;
    QString textAlignment() const;
    int pageWidth() const;
    int scrollSpeed() const;
    QUrl lastBookUrl() const;
    QString librarySortMode() const;
    QString libraryViewMode() const;
    QString settingsFilePath() const;
    QString databaseFilePath() const;
    ProfileDatabase *profileDatabase();
    QVariantMap profileValues() const override;
    bool replaceProfileValues(const QVariantMap &values,
                              QString *errorMessage = nullptr) override;

    void setDarkMode(bool darkMode);
    void setColorTheme(const QString &colorTheme);
    void setLanguage(const QString &language);
    void setReadingFont(const QString &readingFont);
    void setTextFontSize(int textFontSize);
    void setLineHeight(qreal lineHeight);
    void setParagraphSpacing(int paragraphSpacing);
    void setFirstLineIndent(int firstLineIndent);
    void setTextAlignment(const QString &textAlignment);
    void setPageWidth(int pageWidth);
    void setScrollSpeed(int scrollSpeed);
    void setLastBookUrl(const QUrl &lastBookUrl);
    void setLibrarySortMode(const QString &sortMode);
    void setLibraryViewMode(const QString &viewMode);

    Q_INVOKABLE qreal textPosition(const QUrl &documentUrl) const;
    Q_INVOKABLE int textCharacterPosition(const QUrl &documentUrl) const;
    Q_INVOKABLE QString textReadingMode(const QUrl &documentUrl) const;
    Q_INVOKABLE QVariantMap bookTypography(const QUrl &documentUrl) const;
    Q_INVOKABLE int pdfPage(const QUrl &documentUrl) const;
    Q_INVOKABLE qreal pdfScale(const QUrl &documentUrl) const;
    Q_INVOKABLE void saveTextPosition(const QUrl &documentUrl, qreal progress);
    Q_INVOKABLE void saveTextState(const QUrl &documentUrl,
                                   qreal progress,
                                   int characterPosition);
    Q_INVOKABLE void setTextReadingMode(const QUrl &documentUrl,
                                        const QString &readingMode);
    Q_INVOKABLE bool setBookTypography(const QUrl &documentUrl,
                                       const QVariantMap &typography);
    Q_INVOKABLE bool clearBookTypography(const QUrl &documentUrl);
    Q_INVOKABLE void savePdfPosition(const QUrl &documentUrl,
                                     int page,
                                     qreal scale,
                                     qreal progress);
    Q_INVOKABLE void resetReadingPreferences();
    Q_INVOKABLE void sync();

    QVector<LibraryBook> libraryBooks() const;
    void recordBookOpened(const QUrl &documentUrl,
                          const QString &title,
                          const QString &author,
                          const QString &formatName);
    void registerLibraryBook(const QUrl &documentUrl,
                             const QString &title,
                             const QString &author,
                             const QString &formatName,
                             const QUrl &coverUrl,
                             const QString &metadataFingerprint,
                             const QString &collectionPath);
    void updateBookMetadata(const QUrl &documentUrl,
                            const QString &title,
                            const QString &author,
                            const QString &formatName,
                            const QUrl &coverUrl,
                            const QString &metadataFingerprint);
    bool updateBookDetails(const QVector<QUrl> &documentUrls,
                           const BookMetadataPatch &patch,
                           QString *errorMessage = nullptr);
    bool containsLibraryBook(const QUrl &documentUrl) const;
    bool hasLibraryRecord(const QUrl &documentUrl) const;
    void setBookCollection(const QUrl &documentUrl, const QString &collectionPath);
    void removeFromLibrary(const QUrl &documentUrl);
    bool relinkDocument(const QUrl &oldDocumentUrl, const QUrl &newDocumentUrl);

signals:
    void darkModeChanged();
    void colorThemeChanged();
    void languageChanged();
    void readingFontChanged();
    void textFontSizeChanged();
    void lineHeightChanged();
    void paragraphSpacingChanged();
    void firstLineIndentChanged();
    void textAlignmentChanged();
    void pageWidthChanged();
    void scrollSpeedChanged();
    void lastBookUrlChanged();
    void libraryChanged();
    void documentProgressChanged(const QUrl &documentUrl, qreal progress);
    void bookTypographyChanged(const QUrl &documentUrl);
    void profileChanged();
    void profileReplaced();

private:
    static QString defaultSettingsFilePath();
    void loadCachedState();
    void emitProfileSignals();

    mutable QSettings m_settings;
    ProfileDatabase m_profileDatabase;
    QString m_colorTheme = QStringLiteral("light");
    QString m_language = QStringLiteral("system");
    QString m_readingFont = QStringLiteral("serif");
    int m_textFontSize = 18;
    qreal m_lineHeight = 1.5;
    int m_paragraphSpacing = 10;
    int m_firstLineIndent = 24;
    QString m_textAlignment = QStringLiteral("justify");
    int m_pageWidth = 820;
    int m_scrollSpeed = 100;
    QUrl m_lastBookUrl;
    QString m_librarySortMode = QStringLiteral("recent");
    QString m_libraryViewMode = QStringLiteral("grid");
};
