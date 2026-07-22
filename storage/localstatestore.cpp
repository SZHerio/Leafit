#include "localstatestore.h"

#include "../reader/readingtypography.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

namespace {

constexpr int minimumFontSize = ReadingTypography::minimumFontSize;
constexpr int maximumFontSize = ReadingTypography::maximumFontSize;
constexpr qreal minimumLineHeight = ReadingTypography::minimumLineHeight;
constexpr qreal maximumLineHeight = ReadingTypography::maximumLineHeight;
constexpr int minimumParagraphSpacing = ReadingTypography::minimumParagraphSpacing;
constexpr int maximumParagraphSpacing = ReadingTypography::maximumParagraphSpacing;
constexpr int minimumFirstLineIndent = ReadingTypography::minimumFirstLineIndent;
constexpr int maximumFirstLineIndent = ReadingTypography::maximumFirstLineIndent;
constexpr int minimumPageWidth = ReadingTypography::minimumPageWidth;
constexpr int maximumPageWidth = ReadingTypography::maximumPageWidth;
constexpr int minimumScrollSpeed = 50;
constexpr int maximumScrollSpeed = 200;
constexpr int scrollSpeedStep = 10;
constexpr int legacyNormalWheelScrollLines = 6;
const QString defaultColorTheme = QStringLiteral("light");
const QString defaultLanguage = QStringLiteral("system");
const QString defaultReadingFont = QStringLiteral("serif");
const QString defaultTextAlignment = QStringLiteral("justify");

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
    return ReadingTypography::normalizedAlignment(textAlignment);
}

QString normalizedLibrarySortMode(const QString &sortMode)
{
    const QString normalized = sortMode.trimmed().toLower();
    static const QStringList supportedModes = {
        QStringLiteral("recent"),
        QStringLiteral("title"),
        QStringLiteral("author"),
        QStringLiteral("series"),
        QStringLiteral("year")
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
    return ReadingTypography::normalizedFont(readingFont);
}

} // namespace

LocalStateStore::LocalStateStore(QObject *parent)
    : LocalStateStore(defaultSettingsFilePath(), parent)
{
}

LocalStateStore::LocalStateStore(const QString &settingsFilePath, QObject *parent)
    : QObject(parent)
    , m_settings(settingsFilePath, QSettings::IniFormat)
    , m_profileDatabase(ProfileDatabase::databasePathForSettings(settingsFilePath))
{
    QString migrationError;
    if (!m_profileDatabase.migrateLegacySettings(&m_settings, &migrationError)) {
        qWarning() << "Could not migrate the local profile to SQLite:" << migrationError;
    }
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
    m_lastBookUrl = m_profileDatabase.lastBookUrl();
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

QString LocalStateStore::databaseFilePath() const
{
    return m_profileDatabase.filePath();
}

ProfileDatabase *LocalStateStore::profileDatabase()
{
    return &m_profileDatabase;
}

QVariantMap LocalStateStore::profileValues() const
{
    m_settings.sync();
    QVariantMap values = m_profileDatabase.profileValues();
    for (const QString &key : m_settings.allKeys()) {
        values.insert(key, m_settings.value(key));
    }
    return values;
}

bool LocalStateStore::replaceProfileValues(const QVariantMap &values,
                                           QString *errorMessage)
{
    const QVariantMap previousValues = profileValues();
    const auto settingsValues = [](const QVariantMap &profileValues) {
        QVariantMap result;
        for (auto value = profileValues.cbegin(); value != profileValues.cend(); ++value) {
            if (!value.key().startsWith(QStringLiteral("documents/"))
                && !value.key().startsWith(QStringLiteral("annotations/"))
                && value.key() != QLatin1String("session/lastBookUrl")) {
                result.insert(value.key(), value.value());
            }
        }
        return result;
    };
    const auto databaseValues = [](const QVariantMap &profileValues) {
        QVariantMap result;
        for (auto value = profileValues.cbegin(); value != profileValues.cend(); ++value) {
            if (value.key().startsWith(QStringLiteral("documents/"))
                || value.key().startsWith(QStringLiteral("annotations/"))
                || value.key() == QLatin1String("session/lastBookUrl")) {
                result.insert(value.key(), value.value());
            }
        }
        return result;
    };
    const auto writeSettingsValues = [this, &settingsValues](const QVariantMap &profileValues) {
        m_settings.clear();
        const QVariantMap filteredValues = settingsValues(profileValues);
        for (auto value = filteredValues.cbegin(); value != filteredValues.cend(); ++value) {
            m_settings.setValue(value.key(), value.value());
        }
        m_settings.sync();
        return m_settings.status() == QSettings::NoError;
    };

    QString databaseError;
    if (!m_profileDatabase.replaceProfileValues(databaseValues(values), &databaseError)) {
        qWarning() << "Could not replace the SQLite profile:" << databaseError;
        if (errorMessage) {
            *errorMessage = tr("The profile database could not be updated.");
        }
        return false;
    }

    if (!writeSettingsValues(values)) {
        m_profileDatabase.replaceProfileValues(databaseValues(previousValues));
        writeSettingsValues(previousValues);
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

    if (!m_profileDatabase.setLastBookUrl(lastBookUrl)) {
        return;
    }
    m_lastBookUrl = lastBookUrl;
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
    return m_profileDatabase.textPosition(documentUrl);
}

int LocalStateStore::textCharacterPosition(const QUrl &documentUrl) const
{
    return m_profileDatabase.textCharacterPosition(documentUrl);
}

QString LocalStateStore::textReadingMode(const QUrl &documentUrl) const
{
    return m_profileDatabase.textReadingMode(documentUrl);
}

QVariantMap LocalStateStore::bookTypography(const QUrl &documentUrl) const
{
    const std::optional<ReadingTypography> typography =
        m_profileDatabase.bookTypography(documentUrl);
    return typography ? typography->toVariantMap(true)
                      : ReadingTypography().toVariantMap(false);
}

int LocalStateStore::pdfPage(const QUrl &documentUrl) const
{
    return m_profileDatabase.pdfPage(documentUrl);
}

qreal LocalStateStore::pdfScale(const QUrl &documentUrl) const
{
    return m_profileDatabase.pdfScale(documentUrl);
}

void LocalStateStore::saveTextPosition(const QUrl &documentUrl, qreal progress)
{
    saveTextState(documentUrl, progress, -1);
}

void LocalStateStore::saveTextState(const QUrl &documentUrl,
                                    qreal progress,
                                    int characterPosition)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    progress = qBound(qreal(0), progress, qreal(1));
    if (!m_profileDatabase.saveTextPosition(documentUrl,
                                            progress,
                                            characterPosition)) {
        return;
    }
    emit documentProgressChanged(documentUrl, progress);
    emit profileChanged();
}

void LocalStateStore::setTextReadingMode(const QUrl &documentUrl,
                                         const QString &readingMode)
{
    if (documentUrl.isEmpty()
        || !m_profileDatabase.setTextReadingMode(documentUrl, readingMode)) {
        return;
    }
    emit profileChanged();
}

bool LocalStateStore::setBookTypography(const QUrl &documentUrl,
                                        const QVariantMap &typography)
{
    if (documentUrl.isEmpty()
        || !m_profileDatabase.setBookTypography(
            documentUrl,
            ReadingTypography::fromVariantMap(typography))) {
        return false;
    }
    emit bookTypographyChanged(documentUrl);
    emit profileChanged();
    return true;
}

bool LocalStateStore::clearBookTypography(const QUrl &documentUrl)
{
    if (documentUrl.isEmpty()
        || !m_profileDatabase.clearBookTypography(documentUrl)) {
        return false;
    }
    emit bookTypographyChanged(documentUrl);
    emit profileChanged();
    return true;
}

void LocalStateStore::savePdfPosition(const QUrl &documentUrl,
                                      int page,
                                      qreal scale,
                                      qreal progress)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    progress = qBound(qreal(0), progress, qreal(1));
    if (!m_profileDatabase.savePdfPosition(documentUrl, page, scale, progress)) {
        return;
    }
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
    return m_profileDatabase.libraryBooks();
}

void LocalStateStore::recordBookOpened(const QUrl &documentUrl,
                                       const QString &title,
                                       const QString &author,
                                       const QString &formatName)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    if (!m_profileDatabase.recordBookOpened(documentUrl, title, author, formatName)) {
        return;
    }
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

    if (!m_profileDatabase.registerLibraryBook(documentUrl,
                                               title,
                                               author,
                                               formatName,
                                               coverUrl,
                                               metadataFingerprint,
                                               collectionPath)) {
        return;
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

    if (!m_profileDatabase.updateBookMetadata(documentUrl,
                                              title,
                                              author,
                                              formatName,
                                              coverUrl,
                                              metadataFingerprint)) {
        return;
    }
    emit profileChanged();
}

bool LocalStateStore::updateBookDetails(const QVector<QUrl> &documentUrls,
                                        const BookMetadataPatch &patch,
                                        QString *errorMessage)
{
    QString databaseError;
    if (!m_profileDatabase.updateBookDetails(documentUrls, patch, &databaseError)) {
        qWarning() << "Could not update book metadata:" << databaseError;
        if (errorMessage) {
            *errorMessage = tr("Could not update the selected book metadata.");
        }
        return false;
    }
    emit libraryChanged();
    emit profileChanged();
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool LocalStateStore::containsLibraryBook(const QUrl &documentUrl) const
{
    return !documentUrl.isEmpty() && m_profileDatabase.containsLibraryBook(documentUrl);
}

bool LocalStateStore::hasLibraryRecord(const QUrl &documentUrl) const
{
    return !documentUrl.isEmpty() && m_profileDatabase.hasLibraryRecord(documentUrl);
}

void LocalStateStore::setBookCollection(const QUrl &documentUrl,
                                        const QString &collectionPath)
{
    if (documentUrl.isEmpty() || !containsLibraryBook(documentUrl)) {
        return;
    }

    const QString normalizedPath = normalizedCollectionPath(collectionPath);
    if (!m_profileDatabase.setBookCollection(documentUrl, normalizedPath)) {
        return;
    }
    emit libraryChanged();
    emit profileChanged();
}

void LocalStateStore::removeFromLibrary(const QUrl &documentUrl)
{
    if (documentUrl.isEmpty()) {
        return;
    }

    if (!m_profileDatabase.removeFromLibrary(documentUrl)) {
        return;
    }
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

    if (!m_profileDatabase.relinkDocument(oldDocumentUrl, newDocumentUrl)) {
        return false;
    }
    if (m_lastBookUrl == oldDocumentUrl) {
        setLastBookUrl(newDocumentUrl);
    }
    emit libraryChanged();
    emit profileChanged();
    return true;
}

void LocalStateStore::sync()
{
    m_settings.sync();
    m_profileDatabase.sync();
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
