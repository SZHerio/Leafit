#include "desktopintegration.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QSet>
#include <QWindow>

#include <algorithm>
#include <utility>

namespace {

const QString geometryKey = QStringLiteral("desktop/windowGeometry");
const QString screenNameKey = QStringLiteral("desktop/windowScreen");
const QString maximizedKey = QStringLiteral("desktop/windowMaximized");

QString fileIdentity(const QString &filePath)
{
    QString identity = QDir::cleanPath(QFileInfo(filePath).absoluteFilePath());
#ifdef Q_OS_WIN
    identity = identity.toCaseFolded();
#endif
    return identity;
}

qint64 intersectionArea(const QRect &left, const QRect &right)
{
    const QRect intersection = left.intersected(right);
    return qint64(intersection.width()) * qint64(intersection.height());
}

} // namespace

DesktopIntegration::DesktopIntegration(QStringList supportedSuffixes,
                                       const QString &settingsFilePath,
                                       QObject *parent)
    : QObject(parent)
    , m_supportedSuffixes(std::move(supportedSuffixes))
    , m_settings(settingsFilePath.isEmpty()
                     ? std::make_unique<QSettings>()
                     : std::make_unique<QSettings>(settingsFilePath,
                                                   QSettings::IniFormat))
{
    for (QString &suffix : m_supportedSuffixes) {
        suffix = suffix.trimmed().toLower();
    }
    m_supportedSuffixes.removeAll(QString());
    m_supportedSuffixes.removeDuplicates();

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer,
            &QTimer::timeout,
            this,
            &DesktopIntegration::saveWindowState);
    qGuiApp->installEventFilter(this);
    connect(qGuiApp,
            &QGuiApplication::screenRemoved,
            this,
            [this]() {
                QTimer::singleShot(0,
                                   this,
                                   &DesktopIntegration::ensureWindowIsVisible);
            });

    QVariantList arguments;
    const QStringList applicationArguments = QCoreApplication::arguments();
    for (qsizetype index = 1; index < applicationArguments.size(); ++index) {
        const QString argument = applicationArguments.at(index);
        if (argument.startsWith(u'-')) {
            continue;
        }
        arguments.append(QUrl::fromLocalFile(
            QFileInfo(argument).absoluteFilePath()));
    }
    queueBookUrls(arguments);
}

DesktopIntegration::~DesktopIntegration()
{
    qGuiApp->removeEventFilter(this);
}

QStringList DesktopIntegration::supportedSuffixes() const
{
    return m_supportedSuffixes;
}

void DesktopIntegration::attachWindow(QWindow *window)
{
    if (m_window == window) {
        return;
    }
    if (m_window) {
        disconnect(m_window, nullptr, this, nullptr);
    }
    m_window = window;
    if (!m_window) {
        return;
    }

    const QRect storedGeometry = m_settings->value(geometryKey).toRect();
    const QString storedScreenName = m_settings->value(screenNameKey).toString();
    const QList<QRect> screens = orderedAvailableScreens(storedScreenName);
    if (storedGeometry.isValid() && !screens.isEmpty()) {
        m_restoringWindow = true;
        m_normalGeometry = constrainedGeometry(storedGeometry,
                                               screens,
                                               m_window->minimumSize());
        m_window->setGeometry(m_normalGeometry);
        m_restoringWindow = false;
    } else {
        m_normalGeometry = m_window->geometry();
    }

    const auto scheduleSave = [this]() { scheduleWindowSave(); };
    connect(m_window, &QWindow::xChanged, this, scheduleSave);
    connect(m_window, &QWindow::yChanged, this, scheduleSave);
    connect(m_window, &QWindow::widthChanged, this, scheduleSave);
    connect(m_window, &QWindow::heightChanged, this, scheduleSave);
    connect(m_window, &QWindow::screenChanged, this, [this]() {
        ensureWindowIsVisible();
        scheduleWindowSave();
    });
    connect(m_window,
            &QWindow::visibilityChanged,
            this,
            [this](QWindow::Visibility visibility) {
                if (visibility == QWindow::Windowed) {
                    m_normalGeometry = m_window->geometry();
                }
                scheduleWindowSave();
            });

    if (m_settings->value(maximizedKey, false).toBool()) {
        QTimer::singleShot(0, m_window, [window = m_window]() {
            if (window) {
                window->showMaximized();
            }
        });
    }
}

void DesktopIntegration::setReady()
{
    if (m_ready) {
        return;
    }
    m_ready = true;
    if (!m_pendingUrls.isEmpty()) {
        const QVariantList urls = std::exchange(m_pendingUrls, {});
        emit filesOpenRequested(urls);
    }
}

bool DesktopIntegration::isSupportedBookUrl(const QUrl &url) const
{
    if (!url.isLocalFile()) {
        return false;
    }
    const QFileInfo fileInfo(url.toLocalFile());
    return fileInfo.exists()
           && fileInfo.isFile()
           && m_supportedSuffixes.contains(fileInfo.suffix().toLower());
}

QVariantList DesktopIntegration::supportedBookUrls(const QVariantList &values) const
{
    QVariantList urls;
    QSet<QString> identities;
    for (const QVariant &value : values) {
        const QUrl url = value.toUrl();
        if (!isSupportedBookUrl(url)) {
            continue;
        }
        const QFileInfo fileInfo(url.toLocalFile());
        const QString identity = fileIdentity(fileInfo.absoluteFilePath());
        if (identities.contains(identity)) {
            continue;
        }
        identities.insert(identity);
        urls.append(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    }
    return urls;
}

void DesktopIntegration::saveWindowState()
{
    if (!m_window || m_restoringWindow) {
        return;
    }
    if (m_window->visibility() == QWindow::Windowed) {
        m_normalGeometry = m_window->geometry();
    }
    if (m_normalGeometry.isValid()) {
        m_settings->setValue(geometryKey, m_normalGeometry);
    }
    if (m_window->screen()) {
        m_settings->setValue(screenNameKey, m_window->screen()->name());
    }
    m_settings->setValue(maximizedKey,
                         m_window->visibility() == QWindow::Maximized);
    m_settings->sync();
}

QRect DesktopIntegration::constrainedGeometry(
    const QRect &requested,
    const QList<QRect> &availableScreens,
    const QSize &minimumSize)
{
    if (availableScreens.isEmpty()) {
        return requested;
    }

    const QRect *target = &availableScreens.constFirst();
    qint64 bestIntersection = -1;
    for (const QRect &screen : availableScreens) {
        const qint64 area = intersectionArea(requested, screen);
        if (area > bestIntersection) {
            bestIntersection = area;
            target = &screen;
        }
    }

    const int minimumWidth = qMin(qMax(1, minimumSize.width()), target->width());
    const int minimumHeight = qMin(qMax(1, minimumSize.height()), target->height());
    const int width = qBound(minimumWidth,
                             requested.isValid() ? requested.width() : minimumWidth,
                             target->width());
    const int height = qBound(minimumHeight,
                              requested.isValid() ? requested.height() : minimumHeight,
                              target->height());
    const int fallbackX = target->x() + (target->width() - width) / 2;
    const int fallbackY = target->y() + (target->height() - height) / 2;
    const int requestedX = bestIntersection > 0 ? requested.x() : fallbackX;
    const int requestedY = bestIntersection > 0 ? requested.y() : fallbackY;
    const int x = qBound(target->left(), requestedX, target->right() - width + 1);
    const int y = qBound(target->top(), requestedY, target->bottom() - height + 1);
    return {x, y, width, height};
}

bool DesktopIntegration::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::FileOpen) {
        return QObject::eventFilter(watched, event);
    }

    auto *fileOpenEvent = static_cast<QFileOpenEvent *>(event);
    const QUrl url = fileOpenEvent->url().isEmpty()
                         ? QUrl::fromLocalFile(fileOpenEvent->file())
                         : fileOpenEvent->url();
    if (!isSupportedBookUrl(url)) {
        return QObject::eventFilter(watched, event);
    }
    queueBookUrls({url});
    return true;
}

void DesktopIntegration::queueBookUrls(const QVariantList &values)
{
    const QVariantList supported = supportedBookUrls(values);
    if (supported.isEmpty()) {
        return;
    }
    if (m_ready) {
        emit filesOpenRequested(supported);
        return;
    }

    QVariantList combined = m_pendingUrls;
    combined.append(supported);
    m_pendingUrls = supportedBookUrls(combined);
}

void DesktopIntegration::scheduleWindowSave()
{
    if (!m_restoringWindow) {
        m_saveTimer.start();
    }
}

void DesktopIntegration::ensureWindowIsVisible()
{
    if (!m_window || m_window->visibility() != QWindow::Windowed) {
        return;
    }
    const QList<QRect> screens = orderedAvailableScreens({});
    const QRect visibleGeometry = constrainedGeometry(m_window->geometry(),
                                                      screens,
                                                      m_window->minimumSize());
    if (visibleGeometry != m_window->geometry()) {
        m_restoringWindow = true;
        m_window->setGeometry(visibleGeometry);
        m_normalGeometry = visibleGeometry;
        m_restoringWindow = false;
    }
}

QList<QRect> DesktopIntegration::orderedAvailableScreens(
    const QString &preferredScreenName) const
{
    QList<QRect> screens;
    const QList<QScreen *> availableScreens = QGuiApplication::screens();
    const auto preferred = std::find_if(
        availableScreens.cbegin(),
        availableScreens.cend(),
        [&preferredScreenName](const QScreen *screen) {
            return screen && screen->name() == preferredScreenName;
        });
    if (preferred != availableScreens.cend()) {
        screens.append((*preferred)->availableGeometry());
    }
    for (const QScreen *screen : availableScreens) {
        if (screen && (preferred == availableScreens.cend() || screen != *preferred)) {
            screens.append(screen->availableGeometry());
        }
    }
    return screens;
}
