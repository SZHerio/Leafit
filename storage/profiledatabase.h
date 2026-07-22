#pragma once

#include "../library/librarybook.h"
#include "../reader/readingannotation.h"

#include <QSqlDatabase>
#include <QString>
#include <QUrl>
#include <QVariantMap>
#include <QVector>

class QSettings;

class ProfileDatabase final
{
public:
    explicit ProfileDatabase(const QString &databaseFilePath);
    ~ProfileDatabase();

    ProfileDatabase(const ProfileDatabase &) = delete;
    ProfileDatabase &operator=(const ProfileDatabase &) = delete;

    static QString databasePathForSettings(const QString &settingsFilePath);

    bool isOpen() const;
    QString filePath() const;
    QString errorMessage() const;

    bool migrateLegacySettings(QSettings *settings, QString *errorMessage = nullptr);
    QVariantMap profileValues() const;
    bool replaceProfileValues(const QVariantMap &values,
                              QString *errorMessage = nullptr);

    QUrl lastBookUrl() const;
    bool setLastBookUrl(const QUrl &documentUrl);

    qreal textPosition(const QUrl &documentUrl) const;
    int pdfPage(const QUrl &documentUrl) const;
    qreal pdfScale(const QUrl &documentUrl) const;
    bool saveTextPosition(const QUrl &documentUrl, qreal progress);
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
    bool containsLibraryBook(const QUrl &documentUrl) const;
    bool hasLibraryRecord(const QUrl &documentUrl) const;
    bool setBookCollection(const QUrl &documentUrl, const QString &collectionPath);
    bool removeFromLibrary(const QUrl &documentUrl);
    bool relinkDocument(const QUrl &oldDocumentUrl, const QUrl &newDocumentUrl);

    QVector<ReadingAnnotation> annotations(const QUrl &documentUrl) const;
    bool saveAnnotation(const QUrl &documentUrl, const ReadingAnnotation &annotation);
    bool removeAnnotation(const QUrl &documentUrl, const QString &annotationId);

    void sync();

private:
    bool initializeSchema();
    bool createSchema();
    bool ensureBookRecord(const QUrl &documentUrl);
    bool importProfileValues(const QVariantMap &values, QString *errorMessage);
    bool clearProfileData();
    bool execute(const QString &statement) const;
    void setLastError(const QString &message) const;

    QString m_filePath;
    QString m_connectionName;
    QSqlDatabase m_database;
    mutable QString m_errorMessage;
};
