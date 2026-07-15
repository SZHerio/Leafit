#include "../storage/localstatestore.h"
#include "../library/librarymodel.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class LocalStateStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsPreferencesAndLastBook();
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
        store.setDarkMode(true);
        store.setTextFontSize(99);
        store.setLineHeight(9.0);
        store.setPageWidth(1);
        store.setLastBookUrl(bookUrl);
        store.sync();
    }

    LocalStateStore restored(settingsPath);
    QVERIFY(restored.darkMode());
    QCOMPARE(restored.textFontSize(), 36);
    QCOMPARE(restored.lineHeight(), 2.0);
    QCOMPARE(restored.pageWidth(), 560);
    QCOMPARE(restored.lastBookUrl(), bookUrl);
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
    store.recordBookOpened(firstBook, QStringLiteral("First book"), QStringLiteral("TXT"));
    store.saveTextPosition(firstBook, 0.35);
    QTest::qWait(2);
    store.recordBookOpened(secondBook, QStringLiteral("Second book"), QStringLiteral("PDF"));
    store.savePdfPosition(secondBook, 2, 1.2, 0.6);

    const QVector<LibraryBook> books = store.libraryBooks();
    QCOMPARE(books.size(), 2);
    QCOMPARE(books.at(0).sourceUrl, secondBook);
    QCOMPARE(books.at(0).title, QStringLiteral("Second book"));
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
    store.recordBookOpened(textBook, QStringLiteral("Quiet novel"), QStringLiteral("TXT"));
    store.recordBookOpened(pdfBook, QStringLiteral("Reference guide"), QStringLiteral("PDF"));

    LibraryModel model(&store);
    QCOMPARE(model.totalCount(), 2);
    QCOMPARE(model.rowCount(), 2);

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
