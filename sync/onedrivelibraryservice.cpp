#include "onedrivelibraryservice.h"

#include "portableprofilemapper.h"
#include "../library/librarybook.h"
#include "../library/libraryrepository.h"
#include "../storage/localstatestore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace {

const QString metadataDirectoryName = QStringLiteral(".szhbooks");

QString cleanAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString &left, const QString &right)
{
    return cleanAbsolutePath(left).compare(cleanAbsolutePath(right),
#ifdef Q_OS_WIN
                                           Qt::CaseInsensitive
#else
                                           Qt::CaseSensitive
#endif
                                           ) == 0;
}

QString normalizedFileName(QString fileName)
{
    fileName = fileName.trimmed();
    return fileName.isEmpty() ? QStringLiteral("book") : fileName;
}

} // namespace

OneDriveLibraryService::OneDriveLibraryService(LocalStateStore *store,
                                               LibraryRepository *repository,
                                               QObject *parent)
    : OneDriveLibraryService(store, repository, QString(), parent)
{
}

OneDriveLibraryService::OneDriveLibraryService(LocalStateStore *store,
                                               LibraryRepository *repository,
                                               const QString &configurationFilePath,
                                               QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_repository(repository)
    , m_configuration(configurationFilePath)
    , m_profileSync(store)
    , m_rootPath(m_configuration.rootPath())
    , m_suggestedRootPath(suggestedRootPath(&m_oneDriveDetected))
    , m_lastSyncedAt(m_configuration.lastSyncedAt())
{
    Q_ASSERT(m_store);
    Q_ASSERT(m_repository);

    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(650);
    m_profileTimer.setSingleShot(true);
    m_profileTimer.setInterval(1200);

    connect(&m_refreshTimer,
            &QTimer::timeout,
            this,
            &OneDriveLibraryService::synchronizeNow);
    connect(&m_profileTimer,
            &QTimer::timeout,
            this,
            &OneDriveLibraryService::synchronizeNow);
    connect(&m_watcher,
            &QFileSystemWatcher::directoryChanged,
            this,
            [this]() { m_refreshTimer.start(); });
    connect(&m_watcher,
            &QFileSystemWatcher::fileChanged,
            this,
            [this]() { m_refreshTimer.start(); });
    connect(m_store,
            &LocalStateStore::profileChanged,
            this,
            &OneDriveLibraryService::scheduleSynchronization);

    m_available = !m_rootPath.isEmpty()
                  && QFileInfo(m_rootPath).isDir();
    if (configured()) {
        m_status = m_available ? QStringLiteral("pending") : QStringLiteral("error");
    }
    if (m_available) {
        QTimer::singleShot(0, this, &OneDriveLibraryService::synchronizeNow);
    }
}

bool OneDriveLibraryService::configured() const
{
    return !m_rootPath.isEmpty();
}

bool OneDriveLibraryService::available() const
{
    return m_available;
}

QUrl OneDriveLibraryService::rootFolder() const
{
    return m_rootPath.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_rootPath);
}

QString OneDriveLibraryService::rootDisplayPath() const
{
    return QDir::toNativeSeparators(m_rootPath);
}

QUrl OneDriveLibraryService::suggestedFolder() const
{
    return m_suggestedRootPath.isEmpty()
               ? QUrl()
               : QUrl::fromLocalFile(m_suggestedRootPath);
}

QString OneDriveLibraryService::suggestedDisplayPath() const
{
    return QDir::toNativeSeparators(m_suggestedRootPath);
}

bool OneDriveLibraryService::oneDriveDetected() const
{
    return m_oneDriveDetected;
}

QStringList OneDriveLibraryService::collections() const
{
    return m_collections;
}

bool OneDriveLibraryService::syncing() const
{
    return m_syncing;
}

QString OneDriveLibraryService::status() const
{
    return m_status;
}

QString OneDriveLibraryService::errorMessage() const
{
    return m_errorMessage;
}

QDateTime OneDriveLibraryService::lastSyncedAt() const
{
    return m_lastSyncedAt;
}

int OneDriveLibraryService::conflictCount() const
{
    return m_conflictCount;
}

bool OneDriveLibraryService::setRootFolder(const QUrl &folderUrl)
{
    clearError();
    if (!folderUrl.isLocalFile()) {
        setError(tr("Choose a local OneDrive folder."));
        return false;
    }

    const QString rootPath = cleanAbsolutePath(folderUrl.toLocalFile());
    const QFileInfo rootInfo(rootPath);
    if (rootInfo.exists() && !rootInfo.isDir()) {
        setError(tr("The selected path is not a folder."));
        return false;
    }
    if (!QDir().mkpath(rootPath)) {
        setError(tr("Could not create the selected library folder."));
        return false;
    }

    if (configured() && !samePath(m_rootPath, rootPath) && m_available) {
        synchronizeNow();
    }

    const bool rootChanged = m_rootPath.isEmpty() || !samePath(m_rootPath, rootPath);
    const bool availabilityChangedValue = !m_available;
    m_rootPath = rootPath;
    m_available = true;
    m_configuration.setRootPath(rootPath);
    QDir().mkpath(QDir(rootPath).filePath(metadataDirectoryName));
    if (rootChanged) {
        emit rootFolderChanged();
    }
    if (availabilityChangedValue) {
        emit availabilityChanged();
    }

    migrateExistingBooks();
    return synchronizeNow();
}

bool OneDriveLibraryService::useSuggestedFolder()
{
    return setRootFolder(suggestedFolder());
}

bool OneDriveLibraryService::scanNow()
{
    clearError();
    if (!ensureAvailable()) {
        return false;
    }
    const bool scanned = scanLibrary();
    if (scanned) {
        scheduleSynchronization();
    }
    return scanned;
}

bool OneDriveLibraryService::synchronizeNow()
{
    if (m_syncing) {
        return false;
    }

    clearError();
    if (!ensureAvailable()) {
        return false;
    }

    setSyncing(true);
    setStatus(QStringLiteral("syncing"));
    m_profileTimer.stop();
    m_refreshTimer.stop();

    if (!scanLibrary()) {
        setSyncing(false);
        return false;
    }

    m_store->sync();
    const ProfileSyncResult result = m_profileSync.synchronize(
        m_rootPath,
        m_configuration.baseProfile(),
        m_configuration.deviceId());
    if (!result.success) {
        setError(result.errorMessage);
        setSyncing(false);
        refreshWatchPaths();
        return false;
    }

    m_configuration.setBaseProfile(result.mergedProfile);
    m_configuration.setLastSyncedAt(result.syncedAt);
    if (m_lastSyncedAt != result.syncedAt) {
        m_lastSyncedAt = result.syncedAt;
        emit lastSyncedAtChanged();
    }
    if (m_conflictCount != result.conflictKeys.size()) {
        m_conflictCount = result.conflictKeys.size();
        emit conflictCountChanged();
    }
    if (result.localProfileChanged) {
        emit profileApplied();
        scanLibrary();
    }
    if (!result.conflictKeys.isEmpty()) {
        setStatus(QStringLiteral("attention"));
        emit conflictsDetected(QUrl::fromLocalFile(result.conflictFilePath),
                               result.conflictKeys.size());
    } else {
        setStatus(QStringLiteral("synced"));
    }

    refreshWatchPaths();
    setSyncing(false);
    emit synchronizationCompleted();
    return true;
}

QUrl OneDriveLibraryService::importBook(const QUrl &sourceUrl,
                                        const QString &collectionPath)
{
    clearError();
    if (!ensureAvailable()) {
        return {};
    }
    if (!sourceUrl.isLocalFile()) {
        setError(tr("Choose a local book file."));
        return {};
    }

    const QFileInfo sourceInfo(sourceUrl.toLocalFile());
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        setError(tr("The selected book does not exist."));
        return {};
    }
    const QUrl normalizedSource = QUrl::fromLocalFile(sourceInfo.absoluteFilePath());
    if (!m_repository->supports(normalizedSource)) {
        setError(tr("The selected book format is not supported."));
        return {};
    }

    if (PortableProfileMapper::isManagedBook(normalizedSource, m_rootPath)) {
        const QString actualCollection = collectionForBook(normalizedSource);
        m_repository->registerBook(normalizedSource, actualCollection, true);
        scheduleSynchronization();
        return normalizedSource;
    }

    const QString normalizedCollection = normalizedCollectionPath(collectionPath);
    const QString destinationDirectory = absoluteCollectionPath(normalizedCollection);
    if (destinationDirectory.isEmpty() || !QFileInfo(destinationDirectory).isDir()) {
        setError(tr("The selected collection is unavailable."));
        return {};
    }

    const QString destinationPath = destinationForCopy(sourceInfo, normalizedCollection);
    if (destinationPath.isEmpty() || !QFile::copy(sourceInfo.absoluteFilePath(), destinationPath)) {
        setError(tr("Could not copy the book into the OneDrive library."));
        return {};
    }

    const QUrl destinationUrl = QUrl::fromLocalFile(destinationPath);
    m_repository->registerBook(destinationUrl, normalizedCollection, true);
    scanLibrary();
    scheduleSynchronization();
    return destinationUrl;
}

bool OneDriveLibraryService::createCollection(const QString &parentPath,
                                              const QString &name)
{
    clearError();
    if (!ensureAvailable()) {
        return false;
    }

    const QString parent = normalizedCollectionPath(parentPath);
    const QString parentDirectory = absoluteCollectionPath(parent);
    const QString cleanName = name.trimmed();
    if (parentDirectory.isEmpty() || !QFileInfo(parentDirectory).isDir()) {
        setError(tr("The parent collection is unavailable."));
        return false;
    }
    if (!validCollectionName(cleanName)) {
        setError(tr("Use a folder name without reserved characters."));
        return false;
    }

    const QString collectionPath = parent.isEmpty()
                                       ? cleanName
                                       : parent + u'/' + cleanName;
    const QString directoryPath = absoluteCollectionPath(collectionPath);
    if (directoryPath.isEmpty() || QFileInfo::exists(directoryPath)) {
        setError(tr("A collection with this name already exists."));
        return false;
    }
    if (!QDir().mkpath(directoryPath)) {
        setError(tr("Could not create the collection folder."));
        return false;
    }

    scanLibrary();
    refreshWatchPaths();
    return true;
}

QString OneDriveLibraryService::collectionForBook(const QUrl &sourceUrl) const
{
    const QString relativePath = PortableProfileMapper::relativeBookPath(sourceUrl, m_rootPath);
    const qsizetype separator = relativePath.lastIndexOf(u'/');
    return separator < 0 ? QString() : relativePath.left(separator);
}

void OneDriveLibraryService::clearError()
{
    if (m_errorMessage.isEmpty()) {
        return;
    }
    m_errorMessage.clear();
    emit errorMessageChanged();
    if (m_status == QLatin1String("error")) {
        setStatus(configured() ? QStringLiteral("pending")
                               : QStringLiteral("notConfigured"));
    }
}

void OneDriveLibraryService::scheduleSynchronization()
{
    if (!configured()) {
        return;
    }
    if (m_syncing) {
        return;
    }
    m_profileTimer.start();
}

QString OneDriveLibraryService::suggestedRootPath(bool *detected)
{
    for (const char *variable : {"OneDrive", "OneDriveConsumer", "OneDriveCommercial"}) {
        const QString root = qEnvironmentVariable(variable).trimmed();
        if (!root.isEmpty()) {
            if (detected) {
                *detected = true;
            }
            return QDir(root).filePath(QStringLiteral("SZHBooks"));
        }
    }

    if (detected) {
        *detected = false;
    }
    QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (documents.isEmpty()) {
        documents = QDir::homePath();
    }
    return QDir(documents).filePath(QStringLiteral("SZHBooks"));
}

QString OneDriveLibraryService::normalizedCollectionPath(const QString &collectionPath)
{
    QString path = collectionPath.trimmed();
    path.replace(u'\\', u'/');
    path = QDir::cleanPath(path);
    if (path.isEmpty() || path == QLatin1String(".") || path == QLatin1String("all")) {
        return {};
    }
    if (QDir::isAbsolutePath(path)
        || path == QLatin1String("..")
        || path.startsWith(QStringLiteral("../"))
        || path.section(u'/', 0, 0).compare(metadataDirectoryName,
                                            Qt::CaseInsensitive) == 0) {
        return {};
    }
    return QString(path).replace(u'\\', u'/');
}

bool OneDriveLibraryService::validCollectionName(const QString &name)
{
    if (name.isEmpty()
        || name.size() > 120
        || name == QLatin1String(".")
        || name == QLatin1String("..")
        || name.endsWith(u'.')
        || name.endsWith(u' ')
        || name.compare(metadataDirectoryName, Qt::CaseInsensitive) == 0) {
        return false;
    }
    static const QRegularExpression reservedCharacters(
        QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    static const QSet<QString> reservedNames = {
        QStringLiteral("CON"), QStringLiteral("PRN"), QStringLiteral("AUX"),
        QStringLiteral("NUL"), QStringLiteral("COM1"), QStringLiteral("COM2"),
        QStringLiteral("COM3"), QStringLiteral("COM4"), QStringLiteral("COM5"),
        QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"),
        QStringLiteral("LPT3"), QStringLiteral("LPT4"), QStringLiteral("LPT5"),
        QStringLiteral("LPT6"), QStringLiteral("LPT7"), QStringLiteral("LPT8"),
        QStringLiteral("LPT9")
    };
    return !name.contains(reservedCharacters)
           && !reservedNames.contains(name.section(u'.', 0, 0).toUpper());
}

bool OneDriveLibraryService::ensureAvailable()
{
    if (!configured()) {
        setError(tr("Choose a OneDrive library folder first."));
        return false;
    }
    const bool availableNow = QFileInfo(m_rootPath).isDir();
    if (m_available != availableNow) {
        m_available = availableNow;
        emit availabilityChanged();
    }
    if (!availableNow) {
        setError(tr("The OneDrive library folder is unavailable."));
        return false;
    }
    return true;
}

bool OneDriveLibraryService::scanLibrary()
{
    if (!ensureAvailable()) {
        return false;
    }

    QStringList collections;
    scanDirectory(m_rootPath, QString(), &collections);
    std::sort(collections.begin(), collections.end(), [](const QString &left,
                                                         const QString &right) {
        return QString::localeAwareCompare(left, right) < 0;
    });
    if (m_collections != collections) {
        m_collections = collections;
        emit collectionsChanged();
    }
    refreshWatchPaths();
    return true;
}

void OneDriveLibraryService::scanDirectory(const QString &absolutePath,
                                           const QString &relativePath,
                                           QStringList *collections)
{
    const QDir directory(absolutePath);
    const QFileInfoList entries = directory.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);
    for (const QFileInfo &entry : entries) {
        if (entry.isSymLink()) {
            continue;
        }
        const QString childRelativePath = relativePath.isEmpty()
                                              ? entry.fileName()
                                              : relativePath + u'/' + entry.fileName();
        if (entry.isDir()) {
            if (entry.fileName().compare(metadataDirectoryName,
                                         Qt::CaseInsensitive) == 0) {
                continue;
            }
            collections->append(childRelativePath);
            scanDirectory(entry.absoluteFilePath(), childRelativePath, collections);
            continue;
        }

        const QUrl sourceUrl = QUrl::fromLocalFile(entry.absoluteFilePath());
        if (m_repository->supports(sourceUrl)) {
            m_repository->registerBook(sourceUrl, relativePath);
        }
    }
}

QString OneDriveLibraryService::absoluteCollectionPath(const QString &collectionPath) const
{
    const QString normalized = normalizedCollectionPath(collectionPath);
    if (!collectionPath.trimmed().isEmpty()
        && normalized.isEmpty()
        && collectionPath.trimmed() != QLatin1String("all")) {
        return {};
    }

    const QString path = normalized.isEmpty()
                             ? m_rootPath
                             : QDir(m_rootPath).absoluteFilePath(normalized);
    const QString relative = QDir(m_rootPath).relativeFilePath(path);
    if (QDir::isAbsolutePath(relative)
        || relative == QLatin1String("..")
        || relative.startsWith(QStringLiteral("../"))
        || relative.startsWith(QStringLiteral("..\\"))) {
        return {};
    }
    return cleanAbsolutePath(path);
}

QString OneDriveLibraryService::destinationForCopy(const QFileInfo &sourceFile,
                                                   const QString &collectionPath) const
{
    const QString directoryPath = absoluteCollectionPath(collectionPath);
    if (directoryPath.isEmpty()) {
        return {};
    }

    const QString baseName = normalizedFileName(sourceFile.completeBaseName());
    const QString suffix = sourceFile.suffix();
    QString fileName = sourceFile.fileName();
    QString candidate = QDir(directoryPath).filePath(fileName);
    for (int index = 2; QFileInfo::exists(candidate); ++index) {
        fileName = suffix.isEmpty()
                       ? QStringLiteral("%1 (%2)").arg(baseName).arg(index)
                       : QStringLiteral("%1 (%2).%3").arg(baseName).arg(index).arg(suffix);
        candidate = QDir(directoryPath).filePath(fileName);
    }
    return candidate;
}

void OneDriveLibraryService::migrateExistingBooks()
{
    const QVector<LibraryBook> books = m_repository->books();
    QString importedPath;
    for (const LibraryBook &book : books) {
        if (!book.fileAvailable
            || !book.sourceUrl.isLocalFile()
            || PortableProfileMapper::isManagedBook(book.sourceUrl, m_rootPath)) {
            continue;
        }

        if (importedPath.isEmpty()) {
            importedPath = QDir(m_rootPath).filePath(QStringLiteral("Imported"));
            if (!QDir().mkpath(importedPath)) {
                setError(tr("Could not create the Imported collection."));
                return;
            }
        }

        const QFileInfo sourceInfo(book.sourceUrl.toLocalFile());
        const QString destinationPath = destinationForCopy(sourceInfo,
                                                           QStringLiteral("Imported"));
        if (destinationPath.isEmpty()
            || !QFile::copy(sourceInfo.absoluteFilePath(), destinationPath)
            || !m_store->relinkDocument(book.sourceUrl,
                                        QUrl::fromLocalFile(destinationPath))) {
            setError(tr("Some existing books could not be copied into the OneDrive library."));
            continue;
        }
        m_store->setBookCollection(QUrl::fromLocalFile(destinationPath),
                                   QStringLiteral("Imported"));
    }
}

void OneDriveLibraryService::refreshWatchPaths()
{
    if (!m_watcher.files().isEmpty()) {
        m_watcher.removePaths(m_watcher.files());
    }
    if (!m_watcher.directories().isEmpty()) {
        m_watcher.removePaths(m_watcher.directories());
    }
    if (!m_available) {
        return;
    }

    QStringList directories{m_rootPath};
    for (const QString &collection : m_collections) {
        const QString path = absoluteCollectionPath(collection);
        if (!path.isEmpty() && QFileInfo(path).isDir()) {
            directories.append(path);
        }
    }
    const QString metadataPath = QDir(m_rootPath).filePath(metadataDirectoryName);
    if (QFileInfo(metadataPath).isDir()) {
        directories.append(metadataPath);
    }
    m_watcher.addPaths(directories);

    const QString profilePath = ProfileSyncEngine::profileFilePath(m_rootPath);
    if (QFileInfo::exists(profilePath)) {
        m_watcher.addPath(profilePath);
    }
}

void OneDriveLibraryService::setSyncing(bool syncing)
{
    if (m_syncing == syncing) {
        return;
    }
    m_syncing = syncing;
    emit syncingChanged();
}

void OneDriveLibraryService::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged();
}

void OneDriveLibraryService::setError(const QString &errorMessage)
{
    const QString message = errorMessage.trimmed().isEmpty()
                                ? tr("OneDrive synchronization failed.")
                                : errorMessage;
    if (m_errorMessage != message) {
        m_errorMessage = message;
        emit errorMessageChanged();
    }
    setStatus(QStringLiteral("error"));
    emit operationFailed(m_errorMessage);
}
