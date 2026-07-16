#include "../storage/localstatestore.h"
#include "../library/librarymodel.h"

#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class LocalStateStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsPreferencesAndLastBook();
    void migratesLegacyTheme();
    void migratesLegacyScrollSpeed();
    void resetsReadingPreferences();
    void keepsIndependentDocumentPositions();
    void maintainsLocalLibrary();
    void filtersAndRemovesLibraryBooks();
};

void LocalStateStoreTest::persistsPreferencesAndLastBook()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString settingsPath = directory.filePath(QStringLiteral("settings.ini"));
    const QUrl bookUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("book.txt")));

    {
        LocalStateStore store(settingsPath);
        store.setColorTheme(QStringLiteral("sepia"));
        store.setReadingFont(QStringLiteral("sans"));
        store.setTextFontSize(99);
        store.setLineHeight(9.0);
        store.setPageWidth(1);
        store.setScrollSpeed(999);
        store.setLastBookUrl(bookUrl);
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QCOMPARE(restored.colorTheme(), QStringLiteral("sepia"));
    QCOMPARE(restored.readingFont(), QStringLiteral("sans"));
    QVERIFY(!restored.darkMode());
    QCOMPARE(restored.textFontSize(), 36);
    QCOMPARE(restored.lineHeight(), 2.0);
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

void LocalStateStoreTest::resetsReadingPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    LocalStateStore store(directory.filePath(QStringLiteral("settings.ini")));
    store.setColorTheme(QStringLiteral("dark"));
    store.setReadingFont(QStringLiteral("mono"));
    store.setTextFontSize(30);
    store.setLineHeight(1.8);
    store.setPageWidth(1000);
    store.setScrollSpeed(180);

    store.resetReadingPreferences();

    QCOMPARE(store.colorTheme(), QStringLiteral("light"));
    QCOMPARE(store.readingFont(), QStringLiteral("serif"));
    QCOMPARE(store.textFontSize(), 18);
    QCOMPARE(store.lineHeight(), 1.5);
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
        store.saveTextPosition(textBook, 0.42);
        store.savePdfPosition(pdfBook, 17, 1.65, 0.75);
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QVERIFY(qAbs(restored.textPosition(textBook) - 0.42) < 0.0001);
    QCOMPARE(restored.pdfPage(pdfBook), 17);
    QVERIFY(qAbs(restored.pdfScale(pdfBook) - 1.65) < 0.0001);
    QCOMPARE(restored.textPosition(pdfBook), 0.0);
    QCOMPARE(restored.pdfPage(textBook), 0);
    QCOMPARE(restored.pdfScale(textBook), 1.0);
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

    LibraryModel model(&store);
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

    model.removeBook(0);
    QCOMPARE(model.totalCount(), 1);
    QCOMPARE(model.rowCount(), 0);
}

QTEST_GUILESS_MAIN(LocalStateStoreTest)

#include "localstatestoretest.moc"
