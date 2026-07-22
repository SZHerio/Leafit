#include "../diagnostics/diagnosticservice.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class DiagnosticServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void writesAndRotatesLog();
    void exportsCurrentLog();
};

void DiagnosticServiceTest::writesAndRotatesLog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString logPath = directory.filePath(QStringLiteral("szhbooks.log"));
    QFile oversizedLog(logPath);
    QVERIFY(oversizedLog.open(QIODevice::WriteOnly));
    QVERIFY(oversizedLog.resize(2 * 1024 * 1024 + 1));
    oversizedLog.close();

    {
        DiagnosticService service(directory.path());
        qWarning("diagnostic rotation test message");
        QCOMPARE(service.logFileUrl(), QUrl::fromLocalFile(logPath));
        QVERIFY(service.supportSummary().contains(QStringLiteral("SZHBooks")));
    }

    QVERIFY(QFile::exists(logPath + QStringLiteral(".1")));
    QFile currentLog(logPath);
    QVERIFY(currentLog.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(currentLog.readAll().contains("diagnostic rotation test message"));
}

void DiagnosticServiceTest::exportsCurrentLog()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString exportBase = directory.filePath(QStringLiteral("support-copy"));
    {
        DiagnosticService service(directory.filePath(QStringLiteral("logs")));
        qInfo("diagnostic export test message");
        QVERIFY(service.exportCurrentLog(QUrl::fromLocalFile(exportBase)));
    }

    QFile exported(exportBase + QStringLiteral(".log"));
    QVERIFY(exported.open(QIODevice::ReadOnly | QIODevice::Text));
    QVERIFY(exported.readAll().contains("diagnostic export test message"));
}

QTEST_GUILESS_MAIN(DiagnosticServiceTest)

#include "diagnosticservicetest.moc"
