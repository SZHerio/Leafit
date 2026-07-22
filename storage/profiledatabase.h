#pragma once

#include "../library/librarybook.h"
#include "../library/bookmetadatapatch.h"
#include "../reader/readingannotation.h"
#include "../reader/readingtypography.h"

#include <QSqlDatabase>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

#include <optional>

class QSettings;

class ProfileDatabase final
{
public:
    explicit ProfileDatabase(const QString &databaseFilePath);
    ~ProfileDatabase();

    ProfileDatabase(const ProfileDatabase &) = delete;
    ProfileDatabase &operator=(const ProfileDatabase &) = delete;

    static QString databasePathForSettings(const QString &settingsFilePath);
    static QString recoveryBackupPath(const QString &databaseFilePath);
    static QString migrationBackupPath(const QString &databaseFilePath,
                                       int sourceVersion);

    bool isOpen() const;
    QString filePath() const;
    QString errorMessage() const;
    QString recoveryState() const;
    QString recoveryMessage() const;
    QString latestMigrationBackupPath() const;

    bool migrateLegacySettings(QSettings *settings, QString *errorMessage = nullptr);
    QVariantMap profileValues() const;
    bool replaceProfileValues(const QVariantMap &values,
                              QString *errorMessage = nullptr);

    QUrl lastBookUrl() const;
    bool setLastBookUrl(const QUrl &documentUrl);

    qreal textPosition(const QUrl &documentUrl) const;
    int textCharacterPosition(const QUrl &documentUrl) const;
    QString textReadingMode(const QUrl &documentUrl) const;
    std::optional<ReadingTypography> bookTypography(const QUrl &documentUrl) const;
    int pdfPage(const QUrl &documentUrl) const;
    qreal pdfScale(const QUrl &documentUrl) const;
    bool saveTextPosition(const QUrl &documentUrl,
                          qreal progress,
                          int characterPosition = -1);
    bool setTextReadingMode(const QUrl &documentUrl, const QString &readingMode);
    bool setBookTypography(const QUrl &documentUrl,
                           const ReadingTypography &typography);
    bool clearBookTypography(const QUrl &documentUrl);
    bool savePdfPosition(const QUrl &documentUrl,
                         int page,
                         qreal scale,
                         qreal progress);

    QVector<LibraryBook> libraryBooks() const;
    bool recordBookOpened(const QUrl &documentUrl,
                          const QString &title,
                          const QString &author,
                          const QString &formatName);
    bool registerLibraryBook(const QUrl &documentUrl,
                             const QString &title,
                             const QString &author,
                             const QString &formatName,
                             const QUrl &coverUrl,
                             const QString &metadataFingerprint,
                             const QString &collectionPath);
    bool updateBookMetadata(const QUrl &documentUrl,
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
    bool setBookCollection(const QUrl &documentUrl, const QString &collectionPath);
    bool removeFromLibrary(const QUrl &documentUrl);
    bool relinkDocument(const QUrl &oldDocumentUrl, const QUrl &newDocumentUrl);

    QVector<ReadingAnnotation> annotations(const QUrl &documentUrl) const;
    QVector<LibraryReadingAnnotation> libraryAnnotations() const;
    bool saveAnnotation(const QUrl &documentUrl, const ReadingAnnotation &annotation);
    bool removeAnnotation(const QUrl &documentUrl, const QString &annotationId);

    void sync();

private:
    bool openDatabase();
    bool validateIntegrity(QString *errorMessage = nullptr) const;
    bool recoverOrReset(const QString &failureMessage);
    bool restoreBackup(const QString &backupPath,
                       const QString &failureMessage);
    bool initializeSchema();
    bool createSchema();
    int storedSchemaVersion(bool *ok = nullptr) const;
    bool createSnapshot(const QString &destinationPath,
                        QString *errorMessage = nullptr) const;
    bool createMigrationBackup(int sourceVersion);
    qint64 totalChangeCount() const;
    bool ensureBookRecord(const QUrl &documentUrl);
    bool importProfileValues(const QVariantMap &values, QString *errorMessage);
    bool clearProfileData();
    bool execute(const QString &statement) const;
    void setLastError(const QString &message) const;

    QString m_filePath;
    QString m_connectionName;
    QSqlDatabase m_database;
    mutable QString m_errorMessage;
    QString m_recoveryState = QStringLiteral("healthy");
    QString m_recoveryMessage;
    QString m_latestMigrationBackupPath;
    qint64 m_lastSnapshotChangeCount = -1;
};
