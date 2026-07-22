#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

#include <memory>

class QSettings;
class QWindow;

class DesktopIntegration final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList supportedSuffixes READ supportedSuffixes CONSTANT)

public:
    explicit DesktopIntegration(QStringList supportedSuffixes,
                                const QString &settingsFilePath = {},
                                QObject *parent = nullptr);
    ~DesktopIntegration() override;

    QStringList supportedSuffixes() const;

    void attachWindow(QWindow *window);
    void setReady();

    Q_INVOKABLE bool isSupportedBookUrl(const QUrl &url) const;
    Q_INVOKABLE QVariantList supportedBookUrls(const QVariantList &values) const;
    Q_INVOKABLE void saveWindowState();

    static QRect constrainedGeometry(const QRect &requested,
                                     const QList<QRect> &availableScreens,
                                     const QSize &minimumSize);

signals:
    void filesOpenRequested(const QVariantList &urls);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void queueBookUrls(const QVariantList &values);
    void scheduleWindowSave();
    void ensureWindowIsVisible();
    QList<QRect> orderedAvailableScreens(const QString &preferredScreenName) const;

    QStringList m_supportedSuffixes;
    std::unique_ptr<QSettings> m_settings;
    QPointer<QWindow> m_window;
    QTimer m_saveTimer;
    QVariantList m_pendingUrls;
    QRect m_normalGeometry;
    bool m_ready = false;
    bool m_restoringWindow = false;
};
