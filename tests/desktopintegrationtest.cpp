#include "../platform/desktopintegration.h"

#include <QFile>
#include <QFileOpenEvent>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

bool createFile(const QString &filePath)
{
    QFile file(filePath);
    return file.open(QIODevice::WriteOnly) && file.write("test") == 4;
}

} // namespace

class DesktopIntegrationTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndDeduplicatesSupportedFiles();
    void routesFileOpenEventsWhenReady();
    void keepsRestoredWindowsOnAnAvailableScreen();
};

void DesktopIntegrationTest::filtersAndDeduplicatesSupportedFiles()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString textPath = directory.filePath(QStringLiteral("book.txt"));
    const QString epubPath = directory.filePath(QStringLiteral("BOOK.EPUB"));
    const QString binaryPath = directory.filePath(QStringLiteral("tool.exe"));
    QVERIFY(createFile(textPath));
    QVERIFY(createFile(epubPath));
    QVERIFY(createFile(binaryPath));

    DesktopIntegration integration(
        {QStringLiteral("txt"), QStringLiteral("epub")},
        directory.filePath(QStringLiteral("desktop.ini")));
    const QVariantList urls = integration.supportedBookUrls(
        {QUrl::fromLocalFile(textPath),
         QUrl::fromLocalFile(epubPath),
         QUrl::fromLocalFile(binaryPath),
         QUrl::fromLocalFile(textPath),
         QUrl::fromLocalFile(directory.filePath(QStringLiteral("missing.txt")))});

    QCOMPARE(urls.size(), 2);
    QCOMPARE(urls.at(0).toUrl(), QUrl::fromLocalFile(textPath));
    QCOMPARE(urls.at(1).toUrl(), QUrl::fromLocalFile(epubPath));
}

void DesktopIntegrationTest::routesFileOpenEventsWhenReady()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString textPath = directory.filePath(QStringLiteral("opened.txt"));
    QVERIFY(createFile(textPath));

    DesktopIntegration integration(
        {QStringLiteral("txt")},
        directory.filePath(QStringLiteral("desktop.ini")));
    QSignalSpy openSpy(&integration, &DesktopIntegration::filesOpenRequested);
    integration.setReady();

    QFileOpenEvent event(textPath);
    QCoreApplication::sendEvent(qGuiApp, &event);
    QCOMPARE(openSpy.size(), 1);
    const QVariantList urls = openSpy.constFirst().constFirst().toList();
    QCOMPARE(urls.size(), 1);
    QCOMPARE(urls.constFirst().toUrl(), QUrl::fromLocalFile(textPath));
}

void DesktopIntegrationTest::keepsRestoredWindowsOnAnAvailableScreen()
{
    const QList<QRect> screens = {
        QRect(0, 0, 1920, 1040),
        QRect(1920, 0, 1280, 1024)
    };

    const QRect secondScreenWindow = DesktopIntegration::constrainedGeometry(
        QRect(2100, 120, 900, 700), screens, QSize(760, 560));
    QCOMPARE(secondScreenWindow, QRect(2100, 120, 900, 700));

    const QRect disconnectedWindow = DesktopIntegration::constrainedGeometry(
        QRect(5000, 3000, 1000, 800), screens, QSize(760, 560));
    QVERIFY(screens.constFirst().contains(disconnectedWindow));

    const QRect oversizedWindow = DesktopIntegration::constrainedGeometry(
        QRect(2000, -200, 5000, 3000), screens, QSize(760, 560));
    QVERIFY(screens.at(1).contains(oversizedWindow));
    QCOMPARE(oversizedWindow.size(), screens.at(1).size());
}

QTEST_MAIN(DesktopIntegrationTest)

#include "desktopintegrationtest.moc"
