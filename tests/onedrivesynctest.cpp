#include "../library/bookcoverprovider.h"
#include "../library/bookmetadataservice.h"
#include "../library/libraryrepository.h"
#include "../storage/localstatestore.h"
#include "../storage/readingannotationstore.h"
#include "../sync/onedrivelibraryservice.h"
#include "../sync/portableprofilemapper.h"
#include "../sync/profilesyncengine.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
           && file.write(data) == data.size();
}

} // namespace

class OneDriveSyncTest final : public QObject
{
    Q_OBJECT

private slots:
    void mapsManagedBookPathsBetweenDevices();
    void mergesIndependentChangesAndPreservesConflicts();
    void scansNestedCollectionsAndImportsBooks();
    void keepsRootStableDuringBatchImport();
    void writesConflictSnapshotForConcurrentChanges();
    void persistsActivityAndRetriesUnavailableFolder();
};

void OneDriveSyncTest::mapsManagedBookPathsBetweenDevices()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstRoot = directory.filePath(QStringLiteral("device-a/SZHBooks"));
    const QString secondRoot = directory.filePath(QStringLiteral("device-b/SZHBooks"));
    const QString relativePath = QStringLiteral("Fiction/novel.txt");
    QVERIFY(QDir().mkpath(QFileInfo(firstRoot + u'/' + relativePath).absolutePath()));
    QVERIFY(QDir().mkpath(QFileInfo(secondRoot + u'/' + relativePath).absolutePath()));
    QVERIFY(writeFile(firstRoot + u'/' + relativePath, "Portable text"));
    QVERIFY(writeFile(secondRoot + u'/' + relativePath, "Portable text"));

    const QUrl firstBook = QUrl::fromLocalFile(firstRoot + u'/' + relativePath);
    const QUrl secondBook = QUrl::fromLocalFile(secondRoot + u'/' + relativePath);
    LocalStateStore firstStore(directory.filePath(QStringLiteral("first.ini")));
    firstStore.recordBookOpened(firstBook,
                                QStringLiteral("Novel"),
                                QStringLiteral("Writer"),
                                QStringLiteral("TXT"));
    firstStore.setBookCollection(firstBook, QStringLiteral("Fiction"));
    firstStore.saveTextPosition(firstBook, 0.42);
    QVERIFY(firstStore.setBookTypography(firstBook,
                                         {{QStringLiteral("readingFont"),
                                           QStringLiteral("sans")},
                                          {QStringLiteral("fontSize"), 22},
                                          {QStringLiteral("lineHeight"), 1.65},
                                          {QStringLiteral("paragraphSpacing"), 14},
                                          {QStringLiteral("firstLineIndent"), 28},
                                          {QStringLiteral("textAlignment"),
                                           QStringLiteral("left")},
                                          {QStringLiteral("pageWidth"), 780}}));
    firstStore.setLastBookUrl(firstBook);
    const QString relativeCoverPath = QStringLiteral(".szhbooks/covers/novel.png");
    QVERIFY(QDir().mkpath(QFileInfo(firstRoot + u'/' + relativeCoverPath).absolutePath()));
    QVERIFY(QDir().mkpath(QFileInfo(secondRoot + u'/' + relativeCoverPath).absolutePath()));
    QVERIFY(writeFile(firstRoot + u'/' + relativeCoverPath, "custom cover"));
    QVERIFY(writeFile(secondRoot + u'/' + relativeCoverPath, "custom cover"));
    BookMetadataPatch coverPatch;
    coverPatch.customCoverUrl = QUrl::fromLocalFile(firstRoot + u'/' + relativeCoverPath);
    QString coverError;
    QVERIFY2(firstStore.updateBookDetails({firstBook}, coverPatch, &coverError),
             qPrintable(coverError));

    ReadingAnnotationStore firstAnnotations(firstStore.settingsFilePath());
    firstAnnotations.setDocumentUrl(firstBook);
    QVERIFY(firstAnnotations.toggleBookmark(0.42, -1, QStringLiteral("Chapter 4")));
    firstAnnotations.sync();

    const QVariantMap portable = PortableProfileMapper::toPortable(
        firstStore.profileValues(), firstRoot);
    const QByteArray serialized = QJsonDocument(QJsonObject::fromVariantMap(portable))
                                      .toJson(QJsonDocument::Compact);
    QVERIFY(!serialized.contains(firstRoot.toUtf8()));
    QVERIFY(serialized.contains("szhbooks://library/Fiction/novel.txt"));
    QVERIFY(serialized.contains("szhbooks://library/.szhbooks/covers/novel.png"));

    LocalStateStore secondStore(directory.filePath(QStringLiteral("second.ini")));
    const QVariantMap secondProfile = PortableProfileMapper::applyPortable(
        secondStore.profileValues(), portable, secondRoot);
    QString errorMessage;
    QVERIFY2(secondStore.replaceProfileValues(secondProfile, &errorMessage),
             qPrintable(errorMessage));

    const QVector<LibraryBook> books = secondStore.libraryBooks();
    QCOMPARE(books.size(), 1);
    QCOMPARE(books.constFirst().sourceUrl, secondBook);
    QCOMPARE(books.constFirst().collectionPath, QStringLiteral("Fiction"));
    QVERIFY(qAbs(books.constFirst().progress - 0.42) < 0.0001);
    QCOMPARE(books.constFirst().customCoverUrl,
             QUrl::fromLocalFile(secondRoot + u'/' + relativeCoverPath));
    QCOMPARE(secondStore.lastBookUrl(), secondBook);
    const QVariantMap secondTypography = secondStore.bookTypography(secondBook);
    QVERIFY(secondTypography.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(secondTypography.value(QStringLiteral("fontSize")).toInt(), 22);
    QCOMPARE(secondTypography.value(QStringLiteral("pageWidth")).toInt(), 780);

    ReadingAnnotationStore secondAnnotations(secondStore.settingsFilePath());
    secondAnnotations.setDocumentUrl(secondBook);
    QCOMPARE(secondAnnotations.bookmarks().size(), 1);
}

void OneDriveSyncTest::mergesIndependentChangesAndPreservesConflicts()
{
    const QVariantMap base{
        {QStringLiteral("reading/fontSize"), 18},
        {QStringLiteral("appearance/colorTheme"), QStringLiteral("light")}
    };
    QVariantMap local = base;
    local.insert(QStringLiteral("reading/fontSize"), 20);
    QVariantMap remote = base;
    remote.insert(QStringLiteral("appearance/colorTheme"), QStringLiteral("dark"));

    QStringList conflicts;
    QVariantMap merged = ProfileSyncEngine::mergeProfiles(base,
                                                          local,
                                                          remote,
                                                          &conflicts);
    QVERIFY(conflicts.isEmpty());
    QCOMPARE(merged.value(QStringLiteral("reading/fontSize")).toInt(), 20);
    QCOMPARE(merged.value(QStringLiteral("appearance/colorTheme")).toString(),
             QStringLiteral("dark"));

    remote.insert(QStringLiteral("reading/fontSize"), 24);
    merged = ProfileSyncEngine::mergeProfiles(base, local, remote, &conflicts);
    QCOMPARE(conflicts, QStringList{QStringLiteral("reading/fontSize")});
    QCOMPARE(merged.value(QStringLiteral("reading/fontSize")).toInt(), 20);
}

void OneDriveSyncTest::scansNestedCollectionsAndImportsBooks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString rootPath = directory.filePath(QStringLiteral("OneDrive/SZHBooks"));
    QVERIFY(QDir().mkpath(rootPath));
    const QString settingsPath = directory.filePath(QStringLiteral("profile.ini"));
    LocalStateStore store(settingsPath);
    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    OneDriveLibraryService service(&store,
                                   &repository,
                                   directory.filePath(QStringLiteral("sync.ini")));

    QVERIFY(service.setRootFolder(QUrl::fromLocalFile(rootPath)));
    QVERIFY(service.createCollection(QString(), QStringLiteral("Fiction")));
    QVERIFY(service.createCollection(QStringLiteral("Fiction"),
                                     QStringLiteral("Classics")));
    QVERIFY(service.collections().contains(QStringLiteral("Fiction")));
    QVERIFY(service.collections().contains(QStringLiteral("Fiction/Classics")));

    const QString discoveredPath = QDir(rootPath).filePath(
        QStringLiteral("Fiction/discovered.txt"));
    QVERIFY(writeFile(discoveredPath, "Discovered book"));
    QVERIFY(service.synchronizeNow());

    QVector<LibraryBook> books = store.libraryBooks();
    QCOMPARE(books.size(), 1);
    QCOMPARE(books.constFirst().collectionPath, QStringLiteral("Fiction"));

    const QString externalPath = directory.filePath(QStringLiteral("external.txt"));
    QVERIFY(writeFile(externalPath, "Imported book"));
    const QUrl importedUrl = service.importBook(QUrl::fromLocalFile(externalPath),
                                                QStringLiteral("Fiction/Classics"));
    QVERIFY(!importedUrl.isEmpty());
    QVERIFY(QFileInfo::exists(importedUrl.toLocalFile()));
    QCOMPARE(service.collectionForBook(importedUrl), QStringLiteral("Fiction/Classics"));
    QVERIFY(service.synchronizeNow());

    books = store.libraryBooks();
    QCOMPARE(books.size(), 2);
    QVERIFY(QFileInfo::exists(ProfileSyncEngine::profileFilePath(rootPath)));

    store.removeFromLibrary(importedUrl);
    QVERIFY(service.synchronizeNow());
    QCOMPARE(store.libraryBooks().size(), 1);
    QVERIFY(!service.importBook(importedUrl, QStringLiteral("Fiction/Classics")).isEmpty());
    QCOMPARE(store.libraryBooks().size(), 2);
}

void OneDriveSyncTest::keepsRootStableDuringBatchImport()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString rootPath = directory.filePath(QStringLiteral("OneDrive/SZHBooks"));
    const QString nextRootPath = directory.filePath(QStringLiteral("OneDrive/OtherBooks"));
    LocalStateStore store(directory.filePath(QStringLiteral("profile.ini")));
    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    OneDriveLibraryService service(&store,
                                   &repository,
                                   directory.filePath(QStringLiteral("sync.ini")));
    QVERIFY(service.setRootFolder(QUrl::fromLocalFile(rootPath)));

    const QString sourcePath = directory.filePath(QStringLiteral("book.txt"));
    QVERIFY(writeFile(sourcePath, "Book queued for import"));
    QSignalSpy finishedSpy(service.fileService(),
                           &LibraryFileService::batchFinished);
    QVERIFY(service.fileService()->importBooks({QUrl::fromLocalFile(sourcePath)}));
    QVERIFY(service.fileService()->busy());
    QVERIFY(!service.setRootFolder(QUrl::fromLocalFile(nextRootPath)));
    QCOMPARE(service.rootFolder(), QUrl::fromLocalFile(rootPath));
    QVERIFY(!QFileInfo::exists(nextRootPath));

    service.fileService()->cancel();
    QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 10000);
}

void OneDriveSyncTest::writesConflictSnapshotForConcurrentChanges()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString rootPath = directory.filePath(QStringLiteral("OneDrive/SZHBooks"));
    QVERIFY(QDir().mkpath(rootPath));

    LocalStateStore firstStore(directory.filePath(QStringLiteral("first.ini")));
    firstStore.setTextFontSize(18);
    ProfileSyncEngine firstEngine(&firstStore);
    const ProfileSyncResult initial = firstEngine.synchronize(rootPath, {}, QStringLiteral("a"));
    QVERIFY2(initial.success, qPrintable(initial.errorMessage));

    LocalStateStore secondStore(directory.filePath(QStringLiteral("second.ini")));
    QString storageError;
    QVERIFY(secondStore.replaceProfileValues(
        PortableProfileMapper::applyPortable(secondStore.profileValues(),
                                             initial.mergedProfile,
                                             rootPath),
        &storageError));
    secondStore.setTextFontSize(22);
    ProfileSyncEngine secondEngine(&secondStore);
    const ProfileSyncResult remoteChange = secondEngine.synchronize(
        rootPath, initial.mergedProfile, QStringLiteral("b"));
    QVERIFY2(remoteChange.success, qPrintable(remoteChange.errorMessage));

    firstStore.setTextFontSize(24);
    const ProfileSyncResult conflict = firstEngine.synchronize(
        rootPath, initial.mergedProfile, QStringLiteral("a"));
    QVERIFY2(conflict.success, qPrintable(conflict.errorMessage));
    QVERIFY(conflict.conflictKeys.contains(QStringLiteral("reading/fontSize")));
    QVERIFY(QFileInfo::exists(conflict.conflictFilePath));
    QCOMPARE(firstStore.textFontSize(), 24);

    SyncConflictModel conflictModel;
    conflictModel.load(rootPath);
    QVERIFY(conflictModel.rowCount() > 0);
    const QString conflictId = conflictModel.index(0, 0)
                                   .data(SyncConflictModel::ConflictIdRole)
                                   .toString();
    SyncConflictChoice remoteChoice;
    QVERIFY(conflictModel.choice(conflictId, true, &remoteChoice));
    QCOMPARE(remoteChoice.key, QStringLiteral("reading/fontSize"));
    QCOMPARE(remoteChoice.value.toInt(), 22);
    QVERIFY(remoteChoice.present);

    QString resolutionError;
    QVERIFY2(conflictModel.markResolved(conflictId,
                                        QStringLiteral("remote"),
                                        &resolutionError),
             qPrintable(resolutionError));
    QCOMPARE(conflictModel.rowCount(), 0);
}

void OneDriveSyncTest::persistsActivityAndRetriesUnavailableFolder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString configurationPath = directory.filePath(QStringLiteral("sync.ini"));
    {
        SyncActivityModel activity(configurationPath);
        activity.append(QStringLiteral("syncCompleted"),
                        QStringLiteral("info"),
                        QStringLiteral("test"));
        QCOMPARE(activity.rowCount(), 1);
    }
    SyncActivityModel restoredActivity(configurationPath);
    QCOMPARE(restoredActivity.rowCount(), 1);
    QCOMPARE(restoredActivity.index(0, 0).data(SyncActivityModel::EventRole).toString(),
             QStringLiteral("syncCompleted"));

    const QString rootPath = directory.filePath(QStringLiteral("OneDrive/SZHBooks"));
    SyncConfigurationStore configuration(configurationPath);
    configuration.setRootPath(rootPath);
    LocalStateStore store(directory.filePath(QStringLiteral("profile.ini")));
    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    OneDriveLibraryService service(&store, &repository, configurationPath);
    QCOMPARE(service.status(), QStringLiteral("unavailable"));
    QVERIFY(service.retryScheduled());

    QVERIFY(QDir().mkpath(rootPath));
    QVERIFY(service.retryNow());
    QCOMPARE(service.status(), QStringLiteral("synced"));
    QVERIFY(!service.retryScheduled());
}

QTEST_GUILESS_MAIN(OneDriveSyncTest)

#include "onedrivesynctest.moc"
