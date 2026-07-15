#pragma once

#include <QObject>
#include <QSettings>
#include <QUrl>

class LocalStateStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
    Q_PROPERTY(int textFontSize READ textFontSize WRITE setTextFontSize NOTIFY textFontSizeChanged)
    Q_PROPERTY(qreal lineHeight READ lineHeight WRITE setLineHeight NOTIFY lineHeightChanged)
    Q_PROPERTY(int pageWidth READ pageWidth WRITE setPageWidth NOTIFY pageWidthChanged)
    Q_PROPERTY(QUrl lastBookUrl READ lastBookUrl WRITE setLastBookUrl NOTIFY lastBookUrlChanged)

public:
    explicit LocalStateStore(QObject *parent = nullptr);
    explicit LocalStateStore(const QString &settingsFilePath, QObject *parent = nullptr);

    bool darkMode() const;
    int textFontSize() const;
    qreal lineHeight() const;
    int pageWidth() const;
    QUrl lastBookUrl() const;

    void setDarkMode(bool darkMode);
    void setTextFontSize(int textFontSize);
    void setLineHeight(qreal lineHeight);
    void setPageWidth(int pageWidth);
    void setLastBookUrl(const QUrl &lastBookUrl);

    Q_INVOKABLE qreal textPosition(const QUrl &documentUrl) const;
    Q_INVOKABLE int pdfPage(const QUrl &documentUrl) const;
    Q_INVOKABLE qreal pdfScale(const QUrl &documentUrl) const;
    Q_INVOKABLE void saveTextPosition(const QUrl &documentUrl, qreal progress);
    Q_INVOKABLE void savePdfPosition(const QUrl &documentUrl, int page, qreal scale);
    Q_INVOKABLE void sync();

signals:
    void darkModeChanged();
    void textFontSizeChanged();
    void lineHeightChanged();
    void pageWidthChanged();
    void lastBookUrlChanged();

private:
    static QString defaultSettingsFilePath();
    static QString documentId(const QUrl &documentUrl);
    static QString documentKey(const QUrl &documentUrl, const QString &name);
    void rememberDocumentUrl(const QUrl &documentUrl);

    QSettings m_settings;
    bool m_darkMode = false;
    int m_textFontSize = 18;
    qreal m_lineHeight = 1.5;
    int m_pageWidth = 820;
    QUrl m_lastBookUrl;
};
