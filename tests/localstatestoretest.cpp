#include "../storage/localstatestore.h"
#include "../storage/documentstoragekey.h"
#include "../storage/readingannotationstore.h"
#include "../library/bookcoverprovider.h"
#include "../library/bookmetadataservice.h"
#include "../library/librarymodel.h"
#include "../library/libraryrepository.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImage>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

class LocalStateStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsPreferencesAndLastBook();
    void migratesLegacyTheme();
    void migratesRemovedSepiaTheme();
    void migratesLegacyScrollSpeed();
    void migratesProfileDataToSqlite();
    void migratesSqliteMetadataSchema();
    void restoresCorruptDatabaseFromAutomaticBackup();
    void quarantinesDatabaseWhenNoBackupCanRecoverIt();
    void resetsReadingPreferences();
    void keepsIndependentDocumentPositions();
    void persistsBookTypography();
    void persistsLibraryPresentation();
    void maintainsLocalLibrary();
    void filtersAndRemovesLibraryBooks();
    void editsAndFiltersExtendedMetadata();
    void relinksDocumentStateAndAnnotations();
};

void LocalStateStoreTest::persistsPreferencesAndLastBook()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QUrl bookUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("book.txt")));

    {
        LocalStateStore store(settingsPath);
        store.setColorTheme(QStringLiteral("dark"));
        store.setLanguage(QStringLiteral("ru"));
        store.setReadingFont(QStringLiteral("sans"));
        store.setTextFontSize(99);
        store.setLineHeight(9.0);
        store.setParagraphSpacing(99);
        store.setFirstLineIndent(99);
        store.setTextAlignment(QStringLiteral("left"));
        store.setPageWidth(1);
        store.setScrollSpeed(999);
        store.setLastBookUrl(bookUrl);
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QCOMPARE(restored.colorTheme(), QStringLiteral("dark"));
    QCOMPARE(restored.language(), QStringLiteral("ru"));
    QCOMPARE(restored.readingFont(), QStringLiteral("sans"));
    QVERIFY(restored.darkMode());
    QCOMPARE(restored.textFontSize(), 36);
    QCOMPARE(restored.lineHeight(), 2.0);
    QCOMPARE(restored.paragraphSpacing(), 32);
    QCOMPARE(restored.firstLineIndent(), 64);
    QCOMPARE(restored.textAlignment(), QStringLiteral("left"));
    QCOMPARE(restored.pageWidth(), 560);
    QCOMPARE(restored.scrollSpeed(), 200);
    QCOMPARE(restored.lastBookUrl(), bookUrl);
}

void LocalStateStoreTest::migratesLegacyTheme()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/darkMode"), true);
    }

    LocalStateStore store(settingsPath);
    QCOMPARE(store.colorTheme(), QStringLiteral("dark"));
    QVERIFY(store.darkMode());
    store.sync();

    QSettings migratedSettings(settingsPath, QSettings::IniFormat);
    QCOMPARE(migratedSettings.value(QStringLiteral("appearance/colorTheme")).toString(),
             QStringLiteral("dark"));
    QVERIFY(!migratedSettings.contains(QStringLiteral("appearance/darkMode")));
}

void LocalStateStoreTest::migratesRemovedSepiaTheme()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/colorTheme"), QStringLiteral("sepia"));
        settings.setValue(QStringLiteral("appearance/language"), QStringLiteral("unsupported"));
    }

    LocalStateStore store(settingsPath);
    QCOMPARE(store.colorTheme(), QStringLiteral("light"));
    QCOMPARE(store.language(), QStringLiteral("system"));
    QVERIFY(!store.darkMode());
    store.sync();

    QSettings migratedSettings(settingsPath, QSettings::IniFormat);
    QCOMPARE(migratedSettings.value(QStringLiteral("appearance/colorTheme")).toString(),
             QStringLiteral("light"));
    QCOMPARE(migratedSettings.value(QStringLiteral("appearance/language")).toString(),
             QStringLiteral("system"));
}

void LocalStateStoreTest::migratesLegacyScrollSpeed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("reading/wheelScrollLines"), 9);
    }

    LocalStateStore store(settingsPath);
    QCOMPARE(store.scrollSpeed(), 150);
    store.sync();

    QSettings migratedSettings(settingsPath, QSettings::IniFormat);
    QCOMPARE(migratedSettings.value(QStringLiteral("reading/scrollSpeed")).toInt(), 150);
    QVERIFY(!migratedSettings.contains(QStringLiteral("reading/wheelScrollLines")));
}

void LocalStateStoreTest::migratesProfileDataToSqlite()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString bookPath = directory.filePath(QStringLiteral("migrated.txt"));
    QFile bookFile(bookPath);
    QVERIFY(bookFile.open(QIODevice::WriteOnly));
    QVERIFY(bookFile.write("Migrated profile") > 0);
    bookFile.close();

    const QUrl bookUrl = QUrl::fromLocalFile(bookPath);
    const QString documentId = DocumentStorageKey::id(bookUrl);
    const QString documentPrefix = QStringLiteral("documents/%1/").arg(documentId);
    const QString annotationPrefix = QStringLiteral("annotations/%1/").arg(documentId);
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        settings.setValue(QStringLiteral("appearance/colorTheme"), QStringLiteral("dark"));
        settings.setValue(QStringLiteral("session/lastBookUrl"),
                          bookUrl.toString(QUrl::FullyEncoded));
        settings.setValue(documentPrefix + QStringLiteral("sourceUrl"),
                          bookUrl.toString(QUrl::FullyEncoded));
        settings.setValue(documentPrefix + QStringLiteral("inLibrary"), true);
        settings.setValue(documentPrefix + QStringLiteral("title"),
                          QStringLiteral("Migrated title"));
        settings.setValue(documentPrefix + QStringLiteral("author"),
                          QStringLiteral("Migrated author"));
        settings.setValue(documentPrefix + QStringLiteral("formatName"),
                          QStringLiteral("TXT"));
        settings.setValue(documentPrefix + QStringLiteral("textProgress"), 0.37);
        settings.setValue(documentPrefix + QStringLiteral("readingProgress"), 0.37);
        settings.setValue(annotationPrefix + QStringLiteral("sourceUrl"),
                          bookUrl.toString(QUrl::FullyEncoded));
        settings.setValue(annotationPrefix + QStringLiteral("bookmark-1/type"),
                          QStringLiteral("bookmark"));
        settings.setValue(annotationPrefix + QStringLiteral("bookmark-1/progress"), 0.37);
        settings.setValue(annotationPrefix + QStringLiteral("bookmark-1/page"), -1);
        settings.setValue(annotationPrefix + QStringLiteral("bookmark-1/label"),
                          QStringLiteral("Migrated bookmark"));
        settings.sync();
    }

    LocalStateStore store(settingsPath);
    QCOMPARE(store.colorTheme(), QStringLiteral("dark"));
    QCOMPARE(store.lastBookUrl(), bookUrl);
    QCOMPARE(store.libraryBooks().size(), 1);
    QCOMPARE(store.libraryBooks().constFirst().title, QStringLiteral("Migrated title"));
    QVERIFY(qAbs(store.textPosition(bookUrl) - 0.37) < 0.0001);
    QVERIFY(QFileInfo::exists(store.databaseFilePath()));

    ReadingAnnotationStore annotations(store.profileDatabase());
    annotations.setDocumentUrl(bookUrl);
    QCOMPARE(annotations.bookmarks().size(), 1);

    const QVariantMap portableContract = store.profileValues();
    QCOMPARE(portableContract.value(documentPrefix + QStringLiteral("title")).toString(),
             QStringLiteral("Migrated title"));
    QCOMPARE(portableContract.value(annotationPrefix
                                     + QStringLiteral("bookmark-1/label")).toString(),
             QStringLiteral("Migrated bookmark"));

    QSettings migratedSettings(settingsPath, QSettings::IniFormat);
    QVERIFY(!migratedSettings.contains(QStringLiteral("session/lastBookUrl")));
    QVERIFY(!migratedSettings.childGroups().contains(QStringLiteral("documents")));
    QVERIFY(!migratedSettings.childGroups().contains(QStringLiteral("annotations")));
    QCOMPARE(migratedSettings.value(QStringLiteral("appearance/colorTheme")).toString(),
             QStringLiteral("dark"));
}

void LocalStateStoreTest::migratesSqliteMetadataSchema()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString databasePath = ProfileDatabase::databasePathForSettings(settingsPath);
    const QString bookPath = directory.filePath(QStringLiteral("schema-v1.txt"));
    QFile bookFile(bookPath);
    QVERIFY(bookFile.open(QIODevice::WriteOnly));
    QVERIFY(bookFile.write("Schema migration") > 0);
    bookFile.close();
    const QUrl bookUrl = QUrl::fromLocalFile(bookPath);
    const QString documentId = DocumentStorageKey::id(bookUrl);

    const QString connectionName = QStringLiteral("schema-v1-%1").arg(
        QUuid::createUuid().toString(QUuid::WithoutBraces));
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          connectionName);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE schema_meta(key TEXT PRIMARY KEY, value TEXT NOT NULL)")));
        QVERIFY(query.exec(QStringLiteral(
            "INSERT INTO schema_meta(key, value) VALUES('version', '1')")));
        QVERIFY(query.exec(QStringLiteral(
            "CREATE TABLE books("
            "id TEXT PRIMARY KEY, source_url TEXT NOT NULL UNIQUE, "
            "in_library INTEGER NOT NULL DEFAULT 0, title TEXT NOT NULL DEFAULT '', "
            "author TEXT NOT NULL DEFAULT '', format_name TEXT NOT NULL DEFAULT '', "
            "collection_path TEXT NOT NULL DEFAULT '', "
            "metadata_fingerprint TEXT NOT NULL DEFAULT '', cover_url TEXT NOT NULL DEFAULT '', "
            "reading_progress REAL NOT NULL DEFAULT 0, text_progress REAL NOT NULL DEFAULT 0, "
            "pdf_page INTEGER NOT NULL DEFAULT 0, pdf_scale REAL NOT NULL DEFAULT 1, "
            "last_opened TEXT NOT NULL DEFAULT '')")));
        query.prepare(QStringLiteral(
            "INSERT INTO books(id, source_url, in_library, title, format_name) "
            "VALUES(:id, :source_url, 1, 'Version one', 'TXT')"));
        query.bindValue(QStringLiteral(":id"), documentId);
        query.bindValue(QStringLiteral(":source_url"), bookUrl.toString(QUrl::FullyEncoded));
        QVERIFY(query.exec());
        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);

    {
        LocalStateStore store(settingsPath);
        QCOMPARE(store.libraryBooks().size(), 1);
        QCOMPARE(store.libraryBooks().constFirst().series, QString());
        BookMetadataPatch patch;
        patch.series = QStringLiteral("Migrated series");
        patch.tags = QStringList({QStringLiteral("Migrated")});
        QString error;
        QVERIFY2(store.updateBookDetails({bookUrl}, patch, &error), qPrintable(error));
        QCOMPARE(store.libraryBooks().constFirst().series,
                 QStringLiteral("Migrated series"));
        QCOMPARE(store.libraryBooks().constFirst().tags,
                 QStringList({QStringLiteral("Migrated")}));
    }

    const QString migrationBackupPath = ProfileDatabase::migrationBackupPath(databasePath, 1);
    QVERIFY(QFileInfo::exists(migrationBackupPath));
    const QString backupConnection = connectionName + QStringLiteral("-backup");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          backupConnection);
        database.setDatabaseName(migrationBackupPath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "SELECT value FROM schema_meta WHERE key = 'version'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 1);
        database.close();
    }
    QSqlDatabase::removeDatabase(backupConnection);

    const QString verifyConnection = connectionName + QStringLiteral("-verify");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                          verifyConnection);
        database.setDatabaseName(databasePath);
        QVERIFY(database.open());
        QSqlQuery query(database);
        QVERIFY(query.exec(QStringLiteral(
            "SELECT value FROM schema_meta WHERE key = 'version'")));
        QVERIFY(query.next());
        QCOMPARE(query.value(0).toInt(), 4);
        database.close();
    }
    QSqlDatabase::removeDatabase(verifyConnection);
}

void LocalStateStoreTest::restoresCorruptDatabaseFromAutomaticBackup()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString databasePath = ProfileDatabase::databasePathForSettings(settingsPath);
    const QString bookPath = directory.filePath(QStringLiteral("recovered-book.txt"));
    QFile bookFile(bookPath);
    QVERIFY(bookFile.open(QIODevice::WriteOnly));
    QVERIFY(bookFile.write("Recovered book") > 0);
    bookFile.close();
    const QUrl bookUrl = QUrl::fromLocalFile(bookPath);

    {
        LocalStateStore store(settingsPath);
        store.registerLibraryBook(bookUrl,
                                  QStringLiteral("Recovered title"),
                                  QStringLiteral("Author"),
                                  QStringLiteral("TXT"),
                                  {},
                                  QStringLiteral("fingerprint"),
                                  QStringLiteral("Recovered"));
        store.saveTextState(bookUrl, 0.42, 128);
        store.sync();
    }
    QVERIFY(QFileInfo::exists(ProfileDatabase::recoveryBackupPath(databasePath)));

    QFile damaged(databasePath);
    QVERIFY(damaged.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QVERIFY(damaged.write("not a sqlite database") > 0);
    damaged.close();

    LocalStateStore recovered(settingsPath);
    QCOMPARE(recovered.profileRecoveryState(), QStringLiteral("restored"));
    QVERIFY(!recovered.profileRecoveryMessage().isEmpty());
    QCOMPARE(recovered.libraryBooks().size(), 1);
    QCOMPARE(recovered.libraryBooks().constFirst().title, QStringLiteral("Recovered title"));
    QCOMPARE(recovered.textCharacterPosition(bookUrl), 128);
    QVERIFY(!QDir(directory.path())
                 .entryList({QStringLiteral("library.sqlite.corrupt-*")}, QDir::Files)
                 .isEmpty());
}

void LocalStateStoreTest::quarantinesDatabaseWhenNoBackupCanRecoverIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString databasePath = ProfileDatabase::databasePathForSettings(settingsPath);
    QFile damaged(databasePath);
    QVERIFY(damaged.open(QIODevice::WriteOnly));
    QVERIFY(damaged.write("irrecoverable profile") > 0);
    damaged.close();

    LocalStateStore recovered(settingsPath);
    QCOMPARE(recovered.profileRecoveryState(), QStringLiteral("reset"));
    QVERIFY(recovered.profileDatabase()->isOpen());
    QVERIFY(recovered.libraryBooks().isEmpty());
    QVERIFY(!QDir(directory.path())
                 .entryList({QStringLiteral("library.sqlite.corrupt-*")}, QDir::Files)
                 .isEmpty());
}

void LocalStateStoreTest::resetsReadingPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    LocalStateStore store(directory.filePath(QStringLiteral("settings.ini")));
    store.setColorTheme(QStringLiteral("dark"));
    store.setLanguage(QStringLiteral("ru"));
    store.setReadingFont(QStringLiteral("mono"));
    store.setTextFontSize(30);
    store.setLineHeight(1.8);
    store.setParagraphSpacing(28);
    store.setFirstLineIndent(48);
    store.setTextAlignment(QStringLiteral("left"));
    store.setPageWidth(1000);
    store.setScrollSpeed(180);

    store.resetReadingPreferences();

    QCOMPARE(store.colorTheme(), QStringLiteral("light"));
    QCOMPARE(store.language(), QStringLiteral("system"));
    QCOMPARE(store.readingFont(), QStringLiteral("serif"));
    QCOMPARE(store.textFontSize(), 18);
    QCOMPARE(store.lineHeight(), 1.5);
    QCOMPARE(store.paragraphSpacing(), 10);
    QCOMPARE(store.firstLineIndent(), 24);
    QCOMPARE(store.textAlignment(), QStringLiteral("justify"));
    QCOMPARE(store.pageWidth(), 820);
    QCOMPARE(store.scrollSpeed(), 100);
}

void LocalStateStoreTest::keepsIndependentDocumentPositions()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QUrl textBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("story.txt")));
    const QUrl pdfBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("manual.pdf")));

    {
        LocalStateStore store(settingsPath);
        store.saveTextState(textBook, 0.42, 137);
        store.setTextReadingMode(textBook, QStringLiteral("pages"));
        store.savePdfPosition(pdfBook, 17, 1.65, 0.75);
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QVERIFY(qAbs(restored.textPosition(textBook) - 0.42) < 0.0001);
    QCOMPARE(restored.textCharacterPosition(textBook), 137);
    QCOMPARE(restored.textReadingMode(textBook), QStringLiteral("pages"));
    QCOMPARE(restored.pdfPage(pdfBook), 17);
    QVERIFY(qAbs(restored.pdfScale(pdfBook) - 1.65) < 0.0001);
    QCOMPARE(restored.textPosition(pdfBook), 0.0);
    QCOMPARE(restored.pdfPage(textBook), 0);
    QCOMPARE(restored.pdfScale(textBook), 1.0);
    QCOMPARE(restored.textCharacterPosition(pdfBook), -1);
    QCOMPARE(restored.textReadingMode(pdfBook), QStringLiteral("scroll"));
}

void LocalStateStoreTest::persistsBookTypography()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString firstSettings = directory.filePath(QStringLiteral("first.ini"));
    const QString secondSettings = directory.filePath(QStringLiteral("second.ini"));
    const QUrl bookUrl = QUrl::fromLocalFile(
        directory.filePath(QStringLiteral("typography.epub")));
    const QVariantMap typography = {
        {QStringLiteral("readingFont"), QStringLiteral("sans")},
        {QStringLiteral("fontSize"), 23},
        {QStringLiteral("lineHeight"), 1.7},
        {QStringLiteral("paragraphSpacing"), 16},
        {QStringLiteral("firstLineIndent"), 32},
        {QStringLiteral("textAlignment"), QStringLiteral("left")},
        {QStringLiteral("pageWidth"), 760}
    };

    QVariantMap portableProfile;
    {
        LocalStateStore store(firstSettings);
        QVERIFY(store.setBookTypography(bookUrl, typography));
        portableProfile = store.profileValues();
        store.sync();
    }

    LocalStateStore restored(firstSettings);
    const QVariantMap restoredTypography = restored.bookTypography(bookUrl);
    QVERIFY(restoredTypography.value(QStringLiteral("enabled")).toBool());
    QCOMPARE(restoredTypography.value(QStringLiteral("readingFont")).toString(),
             QStringLiteral("sans"));
    QCOMPARE(restoredTypography.value(QStringLiteral("fontSize")).toInt(), 23);
    QCOMPARE(restoredTypography.value(QStringLiteral("lineHeight")).toReal(), 1.7);
    QCOMPARE(restoredTypography.value(QStringLiteral("paragraphSpacing")).toInt(), 16);
    QCOMPARE(restoredTypography.value(QStringLiteral("firstLineIndent")).toInt(), 32);
    QCOMPARE(restoredTypography.value(QStringLiteral("textAlignment")).toString(),
             QStringLiteral("left"));
    QCOMPARE(restoredTypography.value(QStringLiteral("pageWidth")).toInt(), 760);

    LocalStateStore imported(secondSettings);
    QString errorMessage;
    QVERIFY2(imported.replaceProfileValues(portableProfile, &errorMessage),
             qPrintable(errorMessage));
    QVERIFY(imported.bookTypography(bookUrl).value(QStringLiteral("enabled")).toBool());
    QVERIFY(imported.clearBookTypography(bookUrl));
    QVERIFY(!imported.bookTypography(bookUrl).value(QStringLiteral("enabled")).toBool());
}

void LocalStateStoreTest::persistsLibraryPresentation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    {
        LocalStateStore store(settingsPath);
        store.setLibrarySortMode(QStringLiteral("author"));
        store.setLibraryViewMode(QStringLiteral("list"));
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QCOMPARE(restored.librarySortMode(), QStringLiteral("author"));
    QCOMPARE(restored.libraryViewMode(), QStringLiteral("list"));

    restored.setLibrarySortMode(QStringLiteral("unsupported"));
    restored.setLibraryViewMode(QStringLiteral("unsupported"));
    QCOMPARE(restored.librarySortMode(), QStringLiteral("recent"));
    QCOMPARE(restored.libraryViewMode(), QStringLiteral("grid"));
}

void LocalStateStoreTest::maintainsLocalLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.pdf"));
    QFile firstFile(firstPath);
    QVERIFY(firstFile.open(QIODevice::WriteOnly));
    firstFile.close();
    QFile secondFile(secondPath);
    QVERIFY(secondFile.open(QIODevice::WriteOnly));
    secondFile.close();

    const QUrl firstBook = QUrl::fromLocalFile(firstPath);
    const QUrl secondBook = QUrl::fromLocalFile(secondPath);

    LocalStateStore store(settingsPath);
    store.recordBookOpened(firstBook,
                           QStringLiteral("First book"),
                           QStringLiteral("First Author"),
                           QStringLiteral("TXT"));
    store.saveTextPosition(firstBook, 0.35);
    QTest::qWait(2);
    store.recordBookOpened(secondBook,
                           QStringLiteral("Second book"),
                           QStringLiteral("Second Author"),
                           QStringLiteral("PDF"));
    store.savePdfPosition(secondBook, 2, 1.2, 0.6);

    const QVector<LibraryBook> books = store.libraryBooks();
    QCOMPARE(books.size(), 2);
    QCOMPARE(books.at(0).sourceUrl, secondBook);
    QCOMPARE(books.at(0).title, QStringLiteral("Second book"));
    QCOMPARE(books.at(0).author, QStringLiteral("Second Author"));
    QCOMPARE(books.at(0).formatName, QStringLiteral("PDF"));
    QVERIFY(qAbs(books.at(0).progress - 0.6) < 0.0001);
    QVERIFY(books.at(0).fileAvailable);

    store.removeFromLibrary(secondBook);
    const QVector<LibraryBook> remainingBooks = store.libraryBooks();
    QCOMPARE(remainingBooks.size(), 1);
    QCOMPARE(remainingBooks.constFirst().sourceUrl, firstBook);
    QVERIFY(QFileInfo::exists(secondPath));
}

void LocalStateStoreTest::filtersAndRemovesLibraryBooks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QUrl textBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("novel.txt")));
    const QUrl pdfBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("guide.pdf")));

    LocalStateStore store(settingsPath);
    store.recordBookOpened(textBook,
                           QStringLiteral("Quiet novel"),
                           QStringLiteral("Octavia Writer"),
                           QStringLiteral("TXT"));
    store.recordBookOpened(pdfBook,
                           QStringLiteral("Reference guide"),
                           QStringLiteral("Technical Author"),
                           QStringLiteral("PDF"));
    store.setBookCollection(textBook, QStringLiteral("Fiction/Classics"));
    store.setBookCollection(pdfBook, QStringLiteral("Reference"));

    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    LibraryModel model(&repository);
    QCOMPARE(model.totalCount(), 2);
    QCOMPARE(model.rowCount(), 2);

    model.setFilterText(QStringLiteral("Octavia"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::AuthorRole).toString(),
             QStringLiteral("Octavia Writer"));

    model.setFilterText(QStringLiteral("guide"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole).toString(),
             QStringLiteral("Reference guide"));

    model.setFilterText({});
    model.setSortMode(QStringLiteral("title"));
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole).toString(),
             QStringLiteral("Quiet novel"));
    QCOMPARE(model.bookAt(0).value(QStringLiteral("sourceUrl")).toUrl(), textBook);
    QVERIFY(model.bookAt(-1).isEmpty());
    QVERIFY(model.bookAt(model.rowCount()).isEmpty());

    model.setFormatFilter(QStringLiteral("PDF"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::FormatNameRole).toString(),
             QStringLiteral("PDF"));

    model.clearFilters();
    model.setCollectionFilter(QStringLiteral("Fiction"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::CollectionPathRole).toString(),
             QStringLiteral("Fiction/Classics"));

    model.clearFilters();
    store.saveTextPosition(textBook, 0.4);
    model.setProgressFilter(QStringLiteral("reading"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::SourceUrlRole).toUrl(), textBook);

    model.clearFilters();
    model.setFilterText(QStringLiteral("guide"));
    model.removeBook(0);
    QCOMPARE(model.totalCount(), 1);
    QCOMPARE(model.rowCount(), 0);
}

void LocalStateStoreTest::editsAndFiltersExtendedMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString rootPath = directory.filePath(QStringLiteral("SZHBooks"));
    QVERIFY(QDir().mkpath(rootPath));
    const QString firstPath = QDir(rootPath).filePath(QStringLiteral("first.txt"));
    const QString secondPath = QDir(rootPath).filePath(QStringLiteral("second.txt"));
    for (const QString &path : {firstPath, secondPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write("Metadata fixture") > 0);
    }

    const QUrl firstUrl = QUrl::fromLocalFile(firstPath);
    const QUrl secondUrl = QUrl::fromLocalFile(secondPath);
    LocalStateStore store(directory.filePath(QStringLiteral("settings.ini")));
    store.recordBookOpened(firstUrl,
                           QStringLiteral("First parsed title"),
                           QStringLiteral("Parsed author"),
                           QStringLiteral("TXT"));
    store.recordBookOpened(secondUrl,
                           QStringLiteral("Second parsed title"),
                           QStringLiteral("Parsed author"),
                           QStringLiteral("TXT"));

    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    repository.setManagedRootPath(rootPath);
    LibraryModel model(&repository);

    const QVariantMap firstChanges{
        {QStringLiteral("title"), QStringLiteral("User title")},
        {QStringLiteral("author"), QStringLiteral("User author")},
        {QStringLiteral("series"), QStringLiteral("Northern archive")},
        {QStringLiteral("seriesNumber"), 2.0},
        {QStringLiteral("genres"), QStringLiteral("Fiction, Mystery")},
        {QStringLiteral("tags"), QStringLiteral("Favorite; Weekend")},
        {QStringLiteral("language"), QStringLiteral("English")},
        {QStringLiteral("publicationYear"), 2024}
    };
    QVERIFY(model.updateBooksMetadata({firstUrl}, firstChanges));

    QVariantMap firstBook = model.book(firstUrl);
    QCOMPARE(firstBook.value(QStringLiteral("title")).toString(), QStringLiteral("User title"));
    QCOMPARE(firstBook.value(QStringLiteral("author")).toString(), QStringLiteral("User author"));
    QCOMPARE(firstBook.value(QStringLiteral("series")).toString(),
             QStringLiteral("Northern archive"));
    QCOMPARE(firstBook.value(QStringLiteral("genres")).toStringList(),
             QStringList({QStringLiteral("Fiction"), QStringLiteral("Mystery")}));
    QCOMPARE(firstBook.value(QStringLiteral("tags")).toStringList(),
             QStringList({QStringLiteral("Favorite"), QStringLiteral("Weekend")}));
    QCOMPARE(firstBook.value(QStringLiteral("publicationYear")).toInt(), 2024);

    store.updateBookMetadata(firstUrl,
                             QStringLiteral("New parsed title"),
                             QStringLiteral("New parsed author"),
                             QStringLiteral("TXT"),
                             {},
                             QStringLiteral("new-fingerprint"));
    model.refresh();
    firstBook = model.book(firstUrl);
    QCOMPARE(firstBook.value(QStringLiteral("title")).toString(), QStringLiteral("User title"));
    QCOMPARE(firstBook.value(QStringLiteral("author")).toString(), QStringLiteral("User author"));

    const QVariantMap groupChanges{
        {QStringLiteral("tags"), QStringLiteral("Shared, Reference")},
        {QStringLiteral("language"), QStringLiteral("Russian")}
    };
    QVERIFY(model.updateBooksMetadata({firstUrl, secondUrl}, groupChanges));
    QCOMPARE(model.book(firstUrl).value(QStringLiteral("language")).toString(),
             QStringLiteral("Russian"));
    QCOMPARE(model.book(secondUrl).value(QStringLiteral("tags")).toStringList(),
             QStringList({QStringLiteral("Shared"), QStringLiteral("Reference")}));

    model.setTagFilter(QStringLiteral("Shared"));
    QCOMPARE(model.rowCount(), 2);
    model.setGenreFilter(QStringLiteral("Fiction"));
    QCOMPARE(model.rowCount(), 1);
    model.clearFilters();
    model.setLanguageFilter(QStringLiteral("Russian"));
    QCOMPARE(model.rowCount(), 2);
    model.clearFilters();

    const QVariantMap secondChanges{
        {QStringLiteral("series"), QStringLiteral("Northern archive")},
        {QStringLiteral("seriesNumber"), 1.0},
        {QStringLiteral("publicationYear"), 2025}
    };
    QVERIFY(model.updateBooksMetadata({secondUrl}, secondChanges));
    model.setSortMode(QStringLiteral("series"));
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::SourceUrlRole).toUrl(), secondUrl);
    model.setSortMode(QStringLiteral("year"));
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::SourceUrlRole).toUrl(), secondUrl);

    model.setSelectionMode(true);
    model.toggleSelection(firstUrl);
    model.toggleSelection(secondUrl);
    QCOMPARE(model.selectionCount(), 2);
    QCOMPARE(model.selectedBooks().size(), 2);
    model.setSelectionMode(false);
    QCOMPARE(model.selectionCount(), 0);

    const QString sourceCoverPath = directory.filePath(QStringLiteral("cover.png"));
    const QString automaticCoverPath = directory.filePath(QStringLiteral("automatic-cover.png"));
    QImage sourceCover(120, 180, QImage::Format_ARGB32_Premultiplied);
    sourceCover.fill(Qt::black);
    QVERIFY(sourceCover.save(sourceCoverPath, "PNG"));
    sourceCover.fill(Qt::white);
    QVERIFY(sourceCover.save(automaticCoverPath, "PNG"));
    store.updateBookMetadata(firstUrl,
                             QStringLiteral("Ignored parsed title"),
                             QStringLiteral("Ignored parsed author"),
                             QStringLiteral("TXT"),
                             QUrl::fromLocalFile(automaticCoverPath),
                             metadataService.fingerprint(firstUrl));
    QVERIFY(model.setCustomCover(firstUrl, QUrl::fromLocalFile(sourceCoverPath)));
    firstBook = model.book(firstUrl);
    const QUrl customCoverUrl = firstBook.value(QStringLiteral("customCoverUrl")).toUrl();
    QVERIFY(customCoverUrl.isLocalFile());
    QVERIFY(QFileInfo::exists(customCoverUrl.toLocalFile()));
    QVERIFY(QDir(rootPath).relativeFilePath(customCoverUrl.toLocalFile())
                .startsWith(QStringLiteral(".szhbooks/covers/")));
    QVERIFY(QFile::remove(customCoverUrl.toLocalFile()));
    model.refresh();
    firstBook = model.book(firstUrl);
    QCOMPARE(firstBook.value(QStringLiteral("customCoverUrl")).toUrl(), customCoverUrl);
    QCOMPARE(firstBook.value(QStringLiteral("coverUrl")).toUrl(),
             QUrl::fromLocalFile(automaticCoverPath));
}

void LocalStateStoreTest::relinksDocumentStateAndAnnotations()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QString oldPath = directory.filePath(QStringLiteral("old-location.txt"));
    const QString newPath = directory.filePath(QStringLiteral("new-location.txt"));
    const QString existingPath = directory.filePath(QStringLiteral("existing.txt"));
    for (const QString &path : {oldPath, newPath, existingPath}) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("A local-first book."), qint64(19));
    }

    const QUrl oldUrl = QUrl::fromLocalFile(oldPath);
    const QUrl newUrl = QUrl::fromLocalFile(newPath);
    const QUrl existingUrl = QUrl::fromLocalFile(existingPath);
    LocalStateStore store(settingsPath);
    store.recordBookOpened(oldUrl,
                           QStringLiteral("Moved book"),
                           QStringLiteral("Local Author"),
                           QStringLiteral("TXT"));
    store.saveTextPosition(oldUrl, 0.42);
    QVERIFY(store.setBookTypography(oldUrl,
                                    {{QStringLiteral("fontSize"), 21},
                                     {QStringLiteral("pageWidth"), 740}}));
    store.setLastBookUrl(oldUrl);

    ReadingAnnotationStore annotations(settingsPath);
    annotations.setDocumentUrl(oldUrl);
    QVERIFY(annotations.toggleBookmark(0.42, -1, QStringLiteral("Chapter")));
    QVERIFY(!annotations.addHighlight(3,
                                      5,
                                      QStringLiteral("local"),
                                      QStringLiteral("Remember"),
                                      0.42,
                                      -1)
                 .isEmpty());
    annotations.sync();

    QVERIFY(store.relinkDocument(oldUrl, newUrl));
    QCOMPARE(store.lastBookUrl(), newUrl);
    QVERIFY(qAbs(store.textPosition(newUrl) - 0.42) < 0.0001);
    QVERIFY(store.bookTypography(newUrl).value(QStringLiteral("enabled")).toBool());
    QCOMPARE(store.bookTypography(newUrl).value(QStringLiteral("fontSize")).toInt(), 21);
    QVERIFY(!store.bookTypography(oldUrl).value(QStringLiteral("enabled")).toBool());
    QCOMPARE(store.libraryBooks().size(), 1);
    QCOMPARE(store.libraryBooks().constFirst().sourceUrl, newUrl);

    ReadingAnnotationStore restoredAnnotations(settingsPath);
    restoredAnnotations.setDocumentUrl(newUrl);
    QCOMPARE(restoredAnnotations.totalCount(), 2);
    QCOMPARE(restoredAnnotations.bookmarks().size(), 1);
    QCOMPARE(restoredAnnotations.highlights().size(), 1);

    store.recordBookOpened(existingUrl,
                           QStringLiteral("Existing book"),
                           QStringLiteral("Other Author"),
                           QStringLiteral("TXT"));
    store.saveTextPosition(existingUrl, 0.8);

    BookCoverProvider coverProvider(directory.filePath(QStringLiteral("covers")));
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository repository(&store, &metadataService);
    QString errorMessage;
    QVERIFY(!repository.relinkBook(newUrl, existingUrl, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("already")));
    QVERIFY(!store.relinkDocument(newUrl, existingUrl));
    QCOMPARE(store.libraryBooks().size(), 2);
    QVERIFY(qAbs(store.textPosition(newUrl) - 0.42) < 0.0001);
    QVERIFY(qAbs(store.textPosition(existingUrl) - 0.8) < 0.0001);
}

QTEST_GUILESS_MAIN(LocalStateStoreTest)

#include "localstatestoretest.moc"
