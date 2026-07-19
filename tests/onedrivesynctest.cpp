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
    void writesConflictSnapshotForConcurrentChanges();
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
    firstStore.setLastBookUrl(firstBook);

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
    QCOMPARE(secondStore.lastBookUrl(), secondBook);

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
}

QTEST_GUILESS_MAIN(OneDriveSyncTest)

#include "onedrivesynctest.moc"
