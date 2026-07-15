#include "../storage/localstatestore.h"

#include <QTemporaryDir>
#include <QtTest>

class LocalStateStoreTest final : public QObject
{
    Q_OBJECT

private slots:
    void persistsPreferencesAndLastBook();
    void keepsIndependentDocumentPositions();
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
        store.savePdfPosition(pdfBook, 17, 1.65);
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

QTEST_GUILESS_MAIN(LocalStateStoreTest)

#include "localstatestoretest.moc"
