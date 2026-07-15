#include "localstatestore.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace {

constexpr int minimumFontSize = 12;
constexpr int maximumFontSize = 36;
constexpr qreal minimumLineHeight = 1.2;
constexpr qreal maximumLineHeight = 2.0;
constexpr int minimumPageWidth = 560;
constexpr int maximumPageWidth = 1040;
constexpr qreal minimumPdfScale = 0.4;
constexpr qreal maximumPdfScale = 3.0;

QString serializedUrl(const QUrl &url)
{
    return url.toString(QUrl::FullyEncoded);
}

} // namespace

LocalStateStore::LocalStateStore(QObject *parent)
    : LocalStateStore(defaultSettingsFilePath(), parent)
{
}

LocalStateStore::LocalStateStore(const QString &settingsFilePath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsFilePath, QSettings::IniFormat)
{
    m_darkMode = m_settings.value(QStringLiteral("appearance/darkMode"), false).toBool();
    m_textFontSize = qBound(minimumFontSize,
                            m_settings.value(QStringLiteral("reading/fontSize"), 18).toInt(),
                            maximumFontSize);
    m_lineHeight = qBound(minimumLineHeight,
                          m_settings.value(QStringLiteral("reading/lineHeight"), 1.5).toReal(),
                          maximumLineHeight);
    m_pageWidth = qBound(minimumPageWidth,
                         m_settings.value(QStringLiteral("reading/pageWidth"), 820).toInt(),
                         maximumPageWidth);
    m_lastBookUrl = QUrl(m_settings.value(QStringLiteral("session/lastBookUrl")).toString());
}

bool LocalStateStore::darkMode() const
{
    return m_darkMode;
}

int LocalStateStore::textFontSize() const
{
    return m_textFontSize;
}

qreal LocalStateStore::lineHeight() const
{
    return m_lineHeight;
}

int LocalStateStore::pageWidth() const
{
    return m_pageWidth;
}

QUrl LocalStateStore::lastBookUrl() const
{
    return m_lastBookUrl;
}

void LocalStateStore::setDarkMode(bool darkMode)
{
    if (m_darkMode == darkMode) {
        return;
    }

    m_darkMode = darkMode;
    m_settings.setValue(QStringLiteral("appearance/darkMode"), darkMode);
    emit darkModeChanged();
}

void LocalStateStore::setTextFontSize(int textFontSize)
{
    textFontSize = qBound(minimumFontSize, textFontSize, maximumFontSize);
    if (m_textFontSize == textFontSize) {
        return;
    }

    m_textFontSize = textFontSize;
    m_settings.setValue(QStringLiteral("reading/fontSize"), textFontSize);
    emit textFontSizeChanged();
}

void LocalStateStore::setLineHeight(qreal lineHeight)
{
    lineHeight = qBound(minimumLineHeight, lineHeight, maximumLineHeight);
    if (qFuzzyCompare(m_lineHeight, lineHeight)) {
        return;
    }

    m_lineHeight = lineHeight;
    m_settings.setValue(QStringLiteral("reading/lineHeight"), lineHeight);
    emit lineHeightChanged();
}

void LocalStateStore::setPageWidth(int pageWidth)
{
    pageWidth = qBound(minimumPageWidth, pageWidth, maximumPageWidth);
    if (m_pageWidth == pageWidth) {
        return;
    }

    m_pageWidth = pageWidth;
    m_settings.setValue(QStringLiteral("reading/pageWidth"), pageWidth);
    emit pageWidthChanged();
}

void LocalStateStore::setLastBookUrl(const QUrl &lastBookUrl)
{
    if (m_lastBookUrl == lastBookUrl) {
        return;
    }

    m_lastBookUrl = lastBookUrl;
    if (lastBookUrl.isEmpty()) {
        m_settings.remove(QStringLiteral("session/lastBookUrl"));
    } else {
        m_settings.setValue(QStringLiteral("session/lastBookUrl"), serializedUrl(lastBookUrl));
    }
    emit lastBookUrlChanged();
}

qreal LocalStateStore::textPosition(const QUrl &documentUrl) const
{
    return qBound(qreal(0),
                  m_settings.value(documentKey(documentUrl, QStringLiteral("textProgress")), 0).toReal(),
                  qreal(1));
}

int LocalStateStore::pdfPage(const QUrl &documentUrl) const
{
    return qMax(0, m_settings.value(documentKey(documentUrl, QStringLiteral("pdfPage")), 0).toInt());
}

qreal LocalStateStore::pdfScale(const QUrl &documentUrl) const
{
    return qBound(minimumPdfScale,
                  m_settings.value(documentKey(documentUrl, QStringLiteral("pdfScale")), 1).toReal(),
                  maximumPdfScale);
}

void LocalStateStore::saveTextPosition(const QUrl &documentUrl, qreal progress)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    progress = qBound(qreal(0), progress, qreal(1));
    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("textProgress")), progress);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("readingProgress")), progress);
    emit documentProgressChanged(documentUrl, progress);
}

void LocalStateStore::savePdfPosition(const QUrl &documentUrl,
                                      int page,
                                      qreal scale,
                                      qreal progress)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("pdfPage")), qMax(0, page));
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("pdfScale")),
                        qBound(minimumPdfScale, scale, maximumPdfScale));
    progress = qBound(qreal(0), progress, qreal(1));
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("readingProgress")), progress);
    emit documentProgressChanged(documentUrl, progress);
}

QVector<LibraryBook> LocalStateStore::libraryBooks() const
{
    QVector<LibraryBook> books;

    m_settings.beginGroup(QStringLiteral("documents"));
    const QStringList documentIds = m_settings.childGroups();
    books.reserve(documentIds.size());
    for (const QString &documentId : documentIds) {
        m_settings.beginGroup(documentId);
        if (!m_settings.value(QStringLiteral("inLibrary"), false).toBool()) {
            m_settings.endGroup();
            continue;
        }

        LibraryBook book;
        book.sourceUrl = QUrl(m_settings.value(QStringLiteral("sourceUrl")).toString());
        if (book.sourceUrl.isEmpty()) {
            m_settings.endGroup();
            continue;
        }

        const QFileInfo fileInfo(book.sourceUrl.toLocalFile());
        book.sourcePath = book.sourceUrl.isLocalFile()
                              ? fileInfo.absoluteFilePath()
                              : book.sourceUrl.toDisplayString();
        book.title = m_settings.value(QStringLiteral("title")).toString().trimmed();
        if (book.title.isEmpty()) {
            book.title = fileInfo.completeBaseName();
        }
        book.formatName = m_settings.value(QStringLiteral("formatName")).toString().trimmed();
        if (book.formatName.isEmpty()) {
            book.formatName = fileInfo.suffix().toUpper();
        }
        book.progress = qBound(
            qreal(0),
            m_settings.value(QStringLiteral("readingProgress"),
                             m_settings.value(QStringLiteral("textProgress"), 0)).toReal(),
            qreal(1));
        book.lastOpened = QDateTime::fromString(
            m_settings.value(QStringLiteral("lastOpened")).toString(), Qt::ISODateWithMs);
        if (!book.lastOpened.isValid()) {
            book.lastOpened = QDateTime::fromString(
                m_settings.value(QStringLiteral("lastOpened")).toString(), Qt::ISODate);
        }
        book.fileAvailable = !book.sourceUrl.isLocalFile() || fileInfo.exists();
        books.append(book);
        m_settings.endGroup();
    }
    m_settings.endGroup();

    std::sort(books.begin(), books.end(), [](const LibraryBook &left, const LibraryBook &right) {
        if (left.lastOpened != right.lastOpened) {
            return left.lastOpened > right.lastOpened;
        }
        return QString::localeAwareCompare(left.title, right.title) < 0;
    });
    return books;
}

void LocalStateStore::recordBookOpened(const QUrl &documentUrl,
                                       const QString &title,
                                       const QString &formatName)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("inLibrary")), true);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("title")), title.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("formatName")),
                        formatName.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("lastOpened")),
                        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    emit libraryChanged();
}

void LocalStateStore::removeFromLibrary(const QUrl &documentUrl)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    m_settings.setValue(documentKey(documentUrl, QStringLiteral("inLibrary")), false);
    if (m_lastBookUrl == documentUrl) {
        setLastBookUrl({});
    }
    emit libraryChanged();
}

void LocalStateStore::sync()
{
    m_settings.sync();
}

QString LocalStateStore::defaultSettingsFilePath()
{
    const QString overriddenPath = qEnvironmentVariable("LEAFLIT_SETTINGS_FILE");
    if (!overriddenPath.isEmpty()) {
        const QFileInfo fileInfo(overriddenPath);
        QDir().mkpath(fileInfo.absolutePath());
        return fileInfo.absoluteFilePath();
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) {
        directory = QDir::home().filePath(QStringLiteral(".leaflit"));
    }

    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("settings.ini"));
}

QString LocalStateStore::documentId(const QUrl &documentUrl)
{
    QString identity;
    if (documentUrl.isLocalFile()) {
        const QFileInfo fileInfo(documentUrl.toLocalFile());
        identity = fileInfo.canonicalFilePath();
        if (identity.isEmpty()) {
            identity = fileInfo.absoluteFilePath();
        }
        identity = QDir::cleanPath(identity);
#ifdef Q_OS_WIN
        identity = identity.toLower();
#endif
    } else {
        identity = documentUrl.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment)
                       .toString(QUrl::FullyEncoded);
    }

    return QString::fromLatin1(
        QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QString LocalStateStore::documentKey(const QUrl &documentUrl, const QString &name)
{
    return QStringLiteral("documents/%1/%2").arg(documentId(documentUrl), name);
}

void LocalStateStore::rememberDocumentUrl(const QUrl &documentUrl)
{
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("sourceUrl")),
                        serializedUrl(documentUrl));
}
