#include "localstatestore.h"

#include "documentstoragekey.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>

namespace {

constexpr int minimumFontSize = 12;
constexpr int maximumFontSize = 36;
constexpr qreal minimumLineHeight = 1.2;
constexpr qreal maximumLineHeight = 2.0;
constexpr int minimumParagraphSpacing = 0;
constexpr int maximumParagraphSpacing = 32;
constexpr int minimumFirstLineIndent = 0;
constexpr int maximumFirstLineIndent = 64;
constexpr int minimumPageWidth = 560;
constexpr int maximumPageWidth = 1040;
constexpr int minimumScrollSpeed = 50;
constexpr int maximumScrollSpeed = 200;
constexpr int scrollSpeedStep = 10;
constexpr int legacyNormalWheelScrollLines = 6;
constexpr qreal minimumPdfScale = 0.4;
constexpr qreal maximumPdfScale = 3.0;

const QString defaultColorTheme = QStringLiteral("light");
const QString defaultLanguage = QStringLiteral("system");
const QString defaultReadingFont = QStringLiteral("serif");
const QString defaultTextAlignment = QStringLiteral("justify");

QString serializedUrl(const QUrl &url)
{
    return url.toString(QUrl::FullyEncoded);
}

int normalizedScrollSpeed(int scrollSpeed)
{
    const int steppedSpeed = qRound(scrollSpeed / qreal(scrollSpeedStep)) * scrollSpeedStep;
    return qBound(minimumScrollSpeed, steppedSpeed, maximumScrollSpeed);
}

QString normalizedColorTheme(const QString &colorTheme)
{
    const QString normalized = colorTheme.trimmed().toLower();
    static const QStringList supportedThemes = {
        QStringLiteral("light"),
        QStringLiteral("dark")
    };
    return supportedThemes.contains(normalized) ? normalized : defaultColorTheme;
}

QString normalizedLanguage(const QString &language)
{
    const QString normalized = language.trimmed().toLower();
    static const QStringList supportedLanguages = {
        QStringLiteral("system"),
        QStringLiteral("en"),
        QStringLiteral("ru")
    };
    return supportedLanguages.contains(normalized) ? normalized : defaultLanguage;
}

QString normalizedTextAlignment(const QString &textAlignment)
{
    const QString normalized = textAlignment.trimmed().toLower();
    static const QStringList supportedAlignments = {
        QStringLiteral("left"),
        QStringLiteral("justify")
    };
    return supportedAlignments.contains(normalized) ? normalized : defaultTextAlignment;
}

QString normalizedLibrarySortMode(const QString &sortMode)
{
    const QString normalized = sortMode.trimmed().toLower();
    static const QStringList supportedModes = {
        QStringLiteral("recent"),
        QStringLiteral("title"),
        QStringLiteral("author")
    };
    return supportedModes.contains(normalized) ? normalized : QStringLiteral("recent");
}

QString normalizedLibraryViewMode(const QString &viewMode)
{
    return viewMode.trimmed().compare(QStringLiteral("list"), Qt::CaseInsensitive) == 0
               ? QStringLiteral("list")
               : QStringLiteral("grid");
}

QString normalizedCollectionPath(QString collectionPath)
{
    collectionPath.replace(u'\\', u'/');
    collectionPath = QDir::cleanPath(collectionPath.trimmed());
    return collectionPath == QLatin1String(".") ? QString() : collectionPath;
}

QVariantMap settingsGroupValues(QSettings *settings, const QString &group)
{
    QVariantMap values;
    settings->beginGroup(group);
    for (const QString &key : settings->allKeys()) {
        values.insert(key, settings->value(key));
    }
    settings->endGroup();
    return values;
}

void replaceSettingsGroup(QSettings *settings,
                          const QString &group,
                          const QVariantMap &values)
{
    settings->remove(group);
    if (values.isEmpty()) {
        return;
    }

    settings->beginGroup(group);
    for (auto value = values.cbegin(); value != values.cend(); ++value) {
        settings->setValue(value.key(), value.value());
    }
    settings->endGroup();
}

QString configDirectoryForIdentity(const QString &organizationName,
                                   const QString &organizationDomain,
                                   const QString &applicationName)
{
    const QString currentOrganizationName = QCoreApplication::organizationName();
    const QString currentOrganizationDomain = QCoreApplication::organizationDomain();
    const QString currentApplicationName = QCoreApplication::applicationName();

    QCoreApplication::setOrganizationName(organizationName);
    QCoreApplication::setOrganizationDomain(organizationDomain);
    QCoreApplication::setApplicationName(applicationName);
    const QString directory = QStandardPaths::writableLocation(
        QStandardPaths::AppConfigLocation);

    QCoreApplication::setOrganizationName(currentOrganizationName);
    QCoreApplication::setOrganizationDomain(currentOrganizationDomain);
    QCoreApplication::setApplicationName(currentApplicationName);
    return directory;
}

QString migratedDefaultSettingsFilePath()
{
    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) {
        directory = QDir::home().filePath(QStringLiteral(".szhbooks"));
    }

    QDir().mkpath(directory);
    const QString settingsPath = QDir(directory).filePath(QStringLiteral("settings.ini"));
    if (QFileInfo::exists(settingsPath)) {
        return settingsPath;
    }

    QString legacyDirectory = configDirectoryForIdentity(QStringLiteral("Leaflit"),
                                                         QStringLiteral("leaflit.local"),
                                                         QStringLiteral("Leaflit"));
    if (legacyDirectory.isEmpty()) {
        legacyDirectory = QDir::home().filePath(QStringLiteral(".leaflit"));
    }

    const QString legacySettingsPath = QDir(legacyDirectory).filePath(
        QStringLiteral("settings.ini"));
    if (QFileInfo::exists(legacySettingsPath)
        && !QFile::copy(legacySettingsPath, settingsPath)) {
        return legacySettingsPath;
    }
    return settingsPath;
}

QString normalizedReadingFont(const QString &readingFont)
{
    const QString normalized = readingFont.trimmed().toLower();
    static const QStringList supportedFonts = {
        QStringLiteral("serif"),
        QStringLiteral("sans"),
        QStringLiteral("mono")
    };
    return supportedFonts.contains(normalized) ? normalized : defaultReadingFont;
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
    loadCachedState();
}

void LocalStateStore::loadCachedState()
{
    m_colorTheme = defaultColorTheme;
    m_language = defaultLanguage;
    m_readingFont = defaultReadingFont;
    m_textFontSize = 18;
    m_lineHeight = 1.5;
    m_paragraphSpacing = 10;
    m_firstLineIndent = 24;
    m_textAlignment = defaultTextAlignment;
    m_pageWidth = 820;
    m_scrollSpeed = 100;
    m_lastBookUrl = {};
    m_librarySortMode = QStringLiteral("recent");
    m_libraryViewMode = QStringLiteral("grid");

    const QString colorThemeKey = QStringLiteral("appearance/colorTheme");
    const QString legacyDarkModeKey = QStringLiteral("appearance/darkMode");
    if (m_settings.contains(colorThemeKey)) {
        const QString storedColorTheme = m_settings.value(colorThemeKey).toString();
        m_colorTheme = normalizedColorTheme(storedColorTheme);
        if (storedColorTheme != m_colorTheme) {
            m_settings.setValue(colorThemeKey, m_colorTheme);
        }
    } else if (m_settings.contains(legacyDarkModeKey)) {
        m_colorTheme = m_settings.value(legacyDarkModeKey).toBool()
                           ? QStringLiteral("dark")
                           : defaultColorTheme;
        m_settings.setValue(colorThemeKey, m_colorTheme);
    }
    m_settings.remove(legacyDarkModeKey);
    const QString languageKey = QStringLiteral("appearance/language");
    const QString storedLanguage = m_settings.value(languageKey, defaultLanguage).toString();
    m_language = normalizedLanguage(storedLanguage);
    if (storedLanguage != m_language) {
        m_settings.setValue(languageKey, m_language);
    }
    m_readingFont = normalizedReadingFont(
        m_settings.value(QStringLiteral("reading/font"), defaultReadingFont).toString());
    m_textFontSize = qBound(minimumFontSize,
                            m_settings.value(QStringLiteral("reading/fontSize"), 18).toInt(),
                            maximumFontSize);
    m_lineHeight = qBound(minimumLineHeight,
                          m_settings.value(QStringLiteral("reading/lineHeight"), 1.5).toReal(),
                          maximumLineHeight);
    m_paragraphSpacing = qBound(
        minimumParagraphSpacing,
        m_settings.value(QStringLiteral("reading/paragraphSpacing"), 10).toInt(),
        maximumParagraphSpacing);
    m_firstLineIndent = qBound(
        minimumFirstLineIndent,
        m_settings.value(QStringLiteral("reading/firstLineIndent"), 24).toInt(),
        maximumFirstLineIndent);
    m_textAlignment = normalizedTextAlignment(
        m_settings.value(QStringLiteral("reading/textAlignment"),
                         defaultTextAlignment).toString());
    m_pageWidth = qBound(minimumPageWidth,
                         m_settings.value(QStringLiteral("reading/pageWidth"), 820).toInt(),
                         maximumPageWidth);
    const QString scrollSpeedKey = QStringLiteral("reading/scrollSpeed");
    const QString legacyScrollLinesKey = QStringLiteral("reading/wheelScrollLines");
    if (m_settings.contains(scrollSpeedKey)) {
        m_scrollSpeed = normalizedScrollSpeed(m_settings.value(scrollSpeedKey).toInt());
    } else if (m_settings.contains(legacyScrollLinesKey)) {
        const int legacyScrollLines = m_settings.value(legacyScrollLinesKey).toInt();
        m_scrollSpeed = normalizedScrollSpeed(
            qRound(legacyScrollLines * 100.0 / legacyNormalWheelScrollLines));
        m_settings.setValue(scrollSpeedKey, m_scrollSpeed);
        m_settings.remove(legacyScrollLinesKey);
    }
    m_lastBookUrl = QUrl(m_settings.value(QStringLiteral("session/lastBookUrl")).toString());
    m_librarySortMode = normalizedLibrarySortMode(
        m_settings.value(QStringLiteral("library/sortMode"), QStringLiteral("recent")).toString());
    m_libraryViewMode = normalizedLibraryViewMode(
        m_settings.value(QStringLiteral("library/viewMode"), QStringLiteral("grid")).toString());
}

bool LocalStateStore::darkMode() const
{
    return m_colorTheme == QLatin1String("dark");
}

QString LocalStateStore::colorTheme() const
{
    return m_colorTheme;
}

QString LocalStateStore::language() const
{
    return m_language;
}

QString LocalStateStore::readingFont() const
{
    return m_readingFont;
}

int LocalStateStore::textFontSize() const
{
    return m_textFontSize;
}

qreal LocalStateStore::lineHeight() const
{
    return m_lineHeight;
}

int LocalStateStore::paragraphSpacing() const
{
    return m_paragraphSpacing;
}

int LocalStateStore::firstLineIndent() const
{
    return m_firstLineIndent;
}

QString LocalStateStore::textAlignment() const
{
    return m_textAlignment;
}

int LocalStateStore::pageWidth() const
{
    return m_pageWidth;
}

int LocalStateStore::scrollSpeed() const
{
    return m_scrollSpeed;
}

QUrl LocalStateStore::lastBookUrl() const
{
    return m_lastBookUrl;
}

QString LocalStateStore::librarySortMode() const
{
    return m_librarySortMode;
}

QString LocalStateStore::libraryViewMode() const
{
    return m_libraryViewMode;
}

QString LocalStateStore::settingsFilePath() const
{
    return m_settings.fileName();
}

QVariantMap LocalStateStore::profileValues() const
{
    m_settings.sync();
    QVariantMap values;
    for (const QString &key : m_settings.allKeys()) {
        values.insert(key, m_settings.value(key));
    }
    return values;
}

bool LocalStateStore::replaceProfileValues(const QVariantMap &values,
                                           QString *errorMessage)
{
    const QVariantMap previousValues = profileValues();
    const auto writeValues = [this](const QVariantMap &profileValues) {
        m_settings.clear();
        for (auto value = profileValues.cbegin(); value != profileValues.cend(); ++value) {
            m_settings.setValue(value.key(), value.value());
        }
        m_settings.sync();
        return m_settings.status() == QSettings::NoError;
    };

    if (!writeValues(values)) {
        writeValues(previousValues);
        if (errorMessage) {
            *errorMessage = tr("The settings file could not be updated.");
        }
        return false;
    }

    loadCachedState();
    emitProfileSignals();
    emit profileChanged();
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void LocalStateStore::setDarkMode(bool darkMode)
{
    setColorTheme(darkMode ? QStringLiteral("dark") : defaultColorTheme);
}

void LocalStateStore::setColorTheme(const QString &colorTheme)
{
    const QString normalizedTheme = normalizedColorTheme(colorTheme);
    if (m_colorTheme == normalizedTheme) {
        return;
    }

    const bool wasDark = darkMode();
    m_colorTheme = normalizedTheme;
    m_settings.setValue(QStringLiteral("appearance/colorTheme"), normalizedTheme);
    emit colorThemeChanged();
    if (wasDark != darkMode()) {
        emit darkModeChanged();
    }
    emit profileChanged();
}

void LocalStateStore::setLanguage(const QString &language)
{
    const QString normalized = normalizedLanguage(language);
    if (m_language == normalized) {
        return;
    }

    m_language = normalized;
    m_settings.setValue(QStringLiteral("appearance/language"), normalized);
    emit languageChanged();
    emit profileChanged();
}

void LocalStateStore::setReadingFont(const QString &readingFont)
{
    const QString normalizedFont = normalizedReadingFont(readingFont);
    if (m_readingFont == normalizedFont) {
        return;
    }

    m_readingFont = normalizedFont;
    m_settings.setValue(QStringLiteral("reading/font"), normalizedFont);
    emit readingFontChanged();
    emit profileChanged();
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
    emit profileChanged();
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
    emit profileChanged();
}

void LocalStateStore::setParagraphSpacing(int paragraphSpacing)
{
    paragraphSpacing = qBound(minimumParagraphSpacing,
                              paragraphSpacing,
                              maximumParagraphSpacing);
    if (m_paragraphSpacing == paragraphSpacing) {
        return;
    }

    m_paragraphSpacing = paragraphSpacing;
    m_settings.setValue(QStringLiteral("reading/paragraphSpacing"), paragraphSpacing);
    emit paragraphSpacingChanged();
    emit profileChanged();
}

void LocalStateStore::setFirstLineIndent(int firstLineIndent)
{
    firstLineIndent = qBound(minimumFirstLineIndent,
                             firstLineIndent,
                             maximumFirstLineIndent);
    if (m_firstLineIndent == firstLineIndent) {
        return;
    }

    m_firstLineIndent = firstLineIndent;
    m_settings.setValue(QStringLiteral("reading/firstLineIndent"), firstLineIndent);
    emit firstLineIndentChanged();
    emit profileChanged();
}

void LocalStateStore::setTextAlignment(const QString &textAlignment)
{
    const QString normalizedAlignment = normalizedTextAlignment(textAlignment);
    if (m_textAlignment == normalizedAlignment) {
        return;
    }

    m_textAlignment = normalizedAlignment;
    m_settings.setValue(QStringLiteral("reading/textAlignment"), normalizedAlignment);
    emit textAlignmentChanged();
    emit profileChanged();
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
    emit profileChanged();
}

void LocalStateStore::setScrollSpeed(int scrollSpeed)
{
    scrollSpeed = normalizedScrollSpeed(scrollSpeed);
    if (m_scrollSpeed == scrollSpeed) {
        return;
    }

    m_scrollSpeed = scrollSpeed;
    m_settings.setValue(QStringLiteral("reading/scrollSpeed"), scrollSpeed);
    emit scrollSpeedChanged();
    emit profileChanged();
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
    emit profileChanged();
}

void LocalStateStore::setLibrarySortMode(const QString &sortMode)
{
    const QString normalizedMode = normalizedLibrarySortMode(sortMode);
    if (m_librarySortMode == normalizedMode) {
        return;
    }
    m_librarySortMode = normalizedMode;
    m_settings.setValue(QStringLiteral("library/sortMode"), normalizedMode);
    emit profileChanged();
}

void LocalStateStore::setLibraryViewMode(const QString &viewMode)
{
    const QString normalizedMode = normalizedLibraryViewMode(viewMode);
    if (m_libraryViewMode == normalizedMode) {
        return;
    }
    m_libraryViewMode = normalizedMode;
    m_settings.setValue(QStringLiteral("library/viewMode"), normalizedMode);
    emit profileChanged();
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
    emit profileChanged();
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
    emit profileChanged();
}

void LocalStateStore::resetReadingPreferences()
{
    setColorTheme(defaultColorTheme);
    setLanguage(defaultLanguage);
    setReadingFont(defaultReadingFont);
    setTextFontSize(18);
    setLineHeight(1.5);
    setParagraphSpacing(10);
    setFirstLineIndent(24);
    setTextAlignment(defaultTextAlignment);
    setPageWidth(820);
    setScrollSpeed(100);
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
        book.author = m_settings.value(QStringLiteral("author")).toString().trimmed();
        book.formatName = m_settings.value(QStringLiteral("formatName")).toString().trimmed();
        if (book.formatName.isEmpty()) {
            book.formatName = fileInfo.suffix().toUpper();
        }
        book.collectionPath = normalizedCollectionPath(
            m_settings.value(QStringLiteral("collectionPath")).toString());
        book.metadataFingerprint = m_settings.value(
            QStringLiteral("metadataFingerprint")).toString();
        book.coverUrl = QUrl(m_settings.value(QStringLiteral("coverUrl")).toString());
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
                                       const QString &author,
                                       const QString &formatName)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("inLibrary")), true);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("title")), title.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("author")), author.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("formatName")),
                        formatName.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("lastOpened")),
                        QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    emit libraryChanged();
    emit profileChanged();
}

void LocalStateStore::registerLibraryBook(const QUrl &documentUrl,
                                          const QString &title,
                                          const QString &author,
                                          const QString &formatName,
                                          const QUrl &coverUrl,
                                          const QString &metadataFingerprint,
                                          const QString &collectionPath)
{
    if (documentUrl.isEmpty() || hasLibraryRecord(documentUrl)) {
        return;
    }

    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("inLibrary")), true);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("title")), title.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("author")), author.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("formatName")),
                        formatName.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("collectionPath")),
                        normalizedCollectionPath(collectionPath));
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("metadataFingerprint")),
                        metadataFingerprint);
    if (!coverUrl.isEmpty()) {
        m_settings.setValue(documentKey(documentUrl, QStringLiteral("coverUrl")),
                            serializedUrl(coverUrl));
    }
    emit libraryChanged();
    emit profileChanged();
}

void LocalStateStore::updateBookMetadata(const QUrl &documentUrl,
                                         const QString &title,
                                         const QString &author,
                                         const QString &formatName,
                                         const QUrl &coverUrl,
                                         const QString &metadataFingerprint)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    rememberDocumentUrl(documentUrl);
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("title")), title.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("author")), author.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("formatName")),
                        formatName.trimmed());
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("metadataFingerprint")),
                        metadataFingerprint);
    if (coverUrl.isEmpty()) {
        m_settings.remove(documentKey(documentUrl, QStringLiteral("coverUrl")));
    } else {
        m_settings.setValue(documentKey(documentUrl, QStringLiteral("coverUrl")),
                            serializedUrl(coverUrl));
    }
    emit profileChanged();
}

bool LocalStateStore::containsLibraryBook(const QUrl &documentUrl) const
{
    return !documentUrl.isEmpty()
           && m_settings.value(documentKey(documentUrl, QStringLiteral("inLibrary")), false)
                  .toBool();
}

bool LocalStateStore::hasLibraryRecord(const QUrl &documentUrl) const
{
    return !documentUrl.isEmpty()
           && m_settings.contains(documentKey(documentUrl, QStringLiteral("inLibrary")));
}

void LocalStateStore::setBookCollection(const QUrl &documentUrl,
                                        const QString &collectionPath)
{
    if (documentUrl.isEmpty() || !containsLibraryBook(documentUrl)) {
        return;
    }

    const QString key = documentKey(documentUrl, QStringLiteral("collectionPath"));
    const QString normalizedPath = normalizedCollectionPath(collectionPath);
    if (normalizedCollectionPath(m_settings.value(key).toString()) == normalizedPath) {
        return;
    }
    m_settings.setValue(key, normalizedPath);
    emit libraryChanged();
    emit profileChanged();
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
    emit profileChanged();
}

bool LocalStateStore::relinkDocument(const QUrl &oldDocumentUrl,
                                     const QUrl &newDocumentUrl)
{
    if (oldDocumentUrl.isEmpty() || newDocumentUrl.isEmpty()) {
        return false;
    }

    const QString oldId = DocumentStorageKey::id(oldDocumentUrl);
    const QString newId = DocumentStorageKey::id(newDocumentUrl);
    const QString oldDocumentGroup = QStringLiteral("documents/%1").arg(oldId);
    QVariantMap documentValues = settingsGroupValues(&m_settings, oldDocumentGroup);
    if (documentValues.isEmpty()
        || !documentValues.value(QStringLiteral("inLibrary"), false).toBool()) {
        return false;
    }

    documentValues.insert(QStringLiteral("sourceUrl"), serializedUrl(newDocumentUrl));
    documentValues.remove(QStringLiteral("metadataFingerprint"));
    documentValues.remove(QStringLiteral("coverUrl"));

    const QString oldAnnotationGroup = QStringLiteral("annotations/%1").arg(oldId);
    QVariantMap annotationValues = settingsGroupValues(&m_settings, oldAnnotationGroup);
    if (!annotationValues.isEmpty()) {
        annotationValues.insert(QStringLiteral("sourceUrl"), serializedUrl(newDocumentUrl));
    }

    const QString newDocumentGroup = QStringLiteral("documents/%1").arg(newId);
    const QString newAnnotationGroup = QStringLiteral("annotations/%1").arg(newId);
    if (oldId != newId && containsLibraryBook(newDocumentUrl)) {
        return false;
    }

    if (oldId != newId) {
        replaceSettingsGroup(&m_settings, newDocumentGroup, documentValues);
        replaceSettingsGroup(&m_settings, newAnnotationGroup, annotationValues);
        m_settings.remove(oldDocumentGroup);
        m_settings.remove(oldAnnotationGroup);
    } else {
        replaceSettingsGroup(&m_settings, oldDocumentGroup, documentValues);
        replaceSettingsGroup(&m_settings, oldAnnotationGroup, annotationValues);
    }

    if (m_lastBookUrl == oldDocumentUrl) {
        setLastBookUrl(newDocumentUrl);
    }
    m_settings.sync();
    emit libraryChanged();
    emit profileChanged();
    return true;
}

void LocalStateStore::sync()
{
    m_settings.sync();
}

QString LocalStateStore::defaultSettingsFilePath()
{
    QString overriddenPath = qEnvironmentVariable("SZHBOOKS_SETTINGS_FILE");
    if (overriddenPath.isEmpty()) {
        overriddenPath = qEnvironmentVariable("LEAFLIT_SETTINGS_FILE");
    }
    if (!overriddenPath.isEmpty()) {
        const QFileInfo fileInfo(overriddenPath);
        QDir().mkpath(fileInfo.absolutePath());
        return fileInfo.absoluteFilePath();
    }

    return migratedDefaultSettingsFilePath();
}

QString LocalStateStore::documentKey(const QUrl &documentUrl, const QString &name)
{
    return DocumentStorageKey::key(documentUrl, name);
}

void LocalStateStore::rememberDocumentUrl(const QUrl &documentUrl)
{
    m_settings.setValue(documentKey(documentUrl, QStringLiteral("sourceUrl")),
                        serializedUrl(documentUrl));
}

void LocalStateStore::emitProfileSignals()
{
    emit colorThemeChanged();
    emit darkModeChanged();
    emit languageChanged();
    emit readingFontChanged();
    emit textFontSizeChanged();
    emit lineHeightChanged();
    emit paragraphSpacingChanged();
    emit firstLineIndentChanged();
    emit textAlignmentChanged();
    emit pageWidthChanged();
    emit scrollSpeedChanged();
    emit lastBookUrlChanged();
    emit libraryChanged();
    emit profileReplaced();
}
