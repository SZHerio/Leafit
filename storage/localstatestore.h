#pragma once

#include "../library/librarybook.h"

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVector>

class LocalStateStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(QString colorTheme READ colorTheme WRITE setColorTheme NOTIFY colorThemeChanged)
    Q_PROPERTY(QString readingFont READ readingFont WRITE setReadingFont NOTIFY readingFontChanged)
    Q_PROPERTY(int textFontSize READ textFontSize WRITE setTextFontSize NOTIFY textFontSizeChanged)
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY lineHeightChanged)
    Q_PROPERTY(int pageWidth READ pageWidth WRITE setPageWidth NOTIFY pageWidthChanged)
    Q_PROPERTY(int scrollSpeed READ scrollSpeed WRITE setScrollSpeed NOTIFY scrollSpeedChanged)
    Q_PROPERTY(QUrl lastBookUrl READ lastBookUrl WRITE setLastBookUrl NOTIFY lastBookUrlChanged)

public:
    explicit LocalStateStore(QObject *parent = nullptr);
    explicit LocalStateStore(const QString &settingsFilePath, QObject *parent = nullptr);

    bool darkMode() const;
    QString colorTheme() const;
    QString readingFont() const;
    int textFontSize() const;
    qreal lineHeight() const;
    int pageWidth() const;
    int scrollSpeed() const;
    QUrl lastBookUrl() const;

    void setDarkMode(bool darkMode);
    void setColorTheme(const QString &colorTheme);
    void setReadingFont(const QString &readingFont);
    void setTextFontSize(int textFontSize);
    void setLineHeight(qreal lineHeight);
    void setPageWidth(int pageWidth);
    void setScrollSpeed(int scrollSpeed);
    void setLastBookUrl(const QUrl &lastBookUrl);

    Q_INVOKABLE qreal textPosition(const QUrl &documentUrl) const;
    Q_INVOKABLE int pdfPage(const QUrl &documentUrl) const;
    Q_INVOKABLE qreal pdfScale(const QUrl &documentUrl) const;
    Q_INVOKABLE void saveTextPosition(const QUrl &documentUrl, qreal progress);
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
    void removeFromLibrary(const QUrl &documentUrl);

signals:
    void darkModeChanged();
    void colorThemeChanged();
    void readingFontChanged();
    void textFontSizeChanged();
    void lineHeightChanged();
    void pageWidthChanged();
    void scrollSpeedChanged();
    void lastBookUrlChanged();
    void libraryChanged();
    void documentProgressChanged(const QUrl &documentUrl, qreal progress);

private:
    static QString defaultSettingsFilePath();
    static QString documentId(const QUrl &documentUrl);
    static QString documentKey(const QUrl &documentUrl, const QString &name);
    void rememberDocumentUrl(const QUrl &documentUrl);

    mutable QSettings m_settings;
    QString m_colorTheme = QStringLiteral("light");
    QString m_readingFont = QStringLiteral("serif");
    int m_textFontSize = 18;
    qreal m_lineHeight = 1.5;
    int m_pageWidth = 820;
    int m_scrollSpeed = 100;
    QUrl m_lastBookUrl;
};
