#include "diagnosticservice.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {

constexpr qint64 maximumLogSize = 2 * 1024 * 1024;
constexpr int retainedLogCount = 4;

QMutex logMutex;
std::unique_ptr<QFile> activeLog;
QtMessageHandler previousMessageHandler = nullptr;
int installedHandlerCount = 0;

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO ");
    case QtWarningMsg:
        return QStringLiteral("WARN ");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("UNKWN");
}

QString rotatedLogPath(const QString &basePath, int index)
{
    return index == 0
               ? basePath
               : basePath + QStringLiteral(".%1").arg(index);
}

void rotateLogs(const QString &basePath)
{
    if (QFileInfo(basePath).size() < maximumLogSize) {
        return;
    }
    QFile::remove(rotatedLogPath(basePath, retainedLogCount - 1));
    for (int index = retainedLogCount - 2; index >= 0; --index) {
        const QString sourcePath = rotatedLogPath(basePath, index);
        if (QFileInfo::exists(sourcePath)) {
            QFile::rename(sourcePath, rotatedLogPath(basePath, index + 1));
        }
    }
}

void diagnosticMessageHandler(QtMsgType type,
                              const QMessageLogContext &context,
                              const QString &message)
{
    const QByteArray localMessage = message.toUtf8();
    {
        QMutexLocker locker(&logMutex);
        if (activeLog && activeLog->isOpen()) {
            const QString timestamp = QDateTime::currentDateTimeUtc().toString(
                Qt::ISODateWithMs);
            const QString category = context.category && context.category[0] != '\0'
                                         ? QString::fromUtf8(context.category)
                                         : QStringLiteral("default");
            const quintptr threadId = reinterpret_cast<quintptr>(
                QThread::currentThreadId());
            QString line = QStringLiteral("%1 [%2] [t:%3] [%4] %5")
                               .arg(timestamp,
                                    messageTypeName(type),
                                    QString::number(threadId, 16),
                                    category,
                                    message);
            const bool important = type == QtWarningMsg || type == QtCriticalMsg
                                   || type == QtFatalMsg;
            if (context.file && context.line > 0 && important) {
                line += QStringLiteral(" (%1:%2)")
                            .arg(QFileInfo(QString::fromUtf8(context.file)).fileName())
                            .arg(context.line);
            }
            activeLog->write(line.toUtf8());
            activeLog->write("\n");
            if (important) {
                activeLog->flush();
            }
        }
    }

    if (previousMessageHandler) {
        previousMessageHandler(type, context, message);
    }
#ifndef QT_NO_DEBUG
    else {
        std::fprintf(stderr, "%s\n", localMessage.constData());
        std::fflush(stderr);
    }
#endif
    if (type == QtFatalMsg) {
        std::abort();
    }
}

} // namespace

DiagnosticService::DiagnosticService(const QString &logDirectory, QObject *parent)
    : QObject(parent)
    , m_logDirectory(logDirectory.isEmpty()
                         ? defaultLogDirectory()
                         : QFileInfo(logDirectory).absoluteFilePath())
    , m_logFilePath(QDir(m_logDirectory).filePath(QStringLiteral("szhbooks.log")))
{
    installMessageHandler();
    qInfo().noquote() << "SZHBooks" << applicationVersion() << "started on"
                      << QSysInfo::prettyProductName() << '(' << QSysInfo::currentCpuArchitecture()
                      << ") with Qt" << QString::fromLatin1(qVersion());
}

DiagnosticService::~DiagnosticService()
{
    qInfo() << "SZHBooks diagnostic session finished.";
    uninstallMessageHandler();
}

QString DiagnosticService::defaultLogDirectory()
{
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (root.isEmpty()) {
        root = QDir::home().filePath(QStringLiteral(".szhbooks"));
    }
    return QDir(root).filePath(QStringLiteral("logs"));
}

QString DiagnosticService::applicationVersion() const
{
    return QCoreApplication::applicationVersion();
}

QUrl DiagnosticService::logDirectoryUrl() const
{
    return QUrl::fromLocalFile(m_logDirectory);
}

QUrl DiagnosticService::logFileUrl() const
{
    return QUrl::fromLocalFile(m_logFilePath);
}

QString DiagnosticService::supportSummary() const
{
    return QStringLiteral(
               "SZHBooks %1\nQt %2\n%3 (%4)\nLog: %5")
        .arg(applicationVersion(),
             QString::fromLatin1(qVersion()),
             QSysInfo::prettyProductName(),
             QSysInfo::currentCpuArchitecture(),
             QDir::toNativeSeparators(m_logFilePath));
}

bool DiagnosticService::openLogDirectory()
{
    if (QDesktopServices::openUrl(logDirectoryUrl())) {
        return true;
    }
    emit operationFailed(tr("The diagnostics folder could not be opened."));
    return false;
}

bool DiagnosticService::exportCurrentLog(const QUrl &destinationUrl)
{
    QString destinationPath = destinationUrl.toLocalFile();
    if (destinationPath.isEmpty()) {
        emit operationFailed(tr("Choose a local destination for the diagnostic log."));
        return false;
    }
    if (!destinationPath.endsWith(QStringLiteral(".log"), Qt::CaseInsensitive)) {
        destinationPath += QStringLiteral(".log");
    }

    {
        QMutexLocker locker(&logMutex);
        if (activeLog) {
            activeLog->flush();
        }
    }
    QFile::remove(destinationPath);
    if (!QFile::copy(m_logFilePath, destinationPath)) {
        emit operationFailed(tr("The diagnostic log could not be exported."));
        return false;
    }
    return true;
}

void DiagnosticService::installMessageHandler()
{
    QMutexLocker locker(&logMutex);
    if (!QDir().mkpath(m_logDirectory)) {
        return;
    }
    if (installedHandlerCount == 0) {
        rotateLogs(m_logFilePath);
        activeLog = std::make_unique<QFile>(m_logFilePath);
        if (!activeLog->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            activeLog.reset();
            return;
        }
        previousMessageHandler = qInstallMessageHandler(diagnosticMessageHandler);
    }
    ++installedHandlerCount;
    m_handlerInstalled = true;
}

void DiagnosticService::uninstallMessageHandler()
{
    if (!m_handlerInstalled) {
        return;
    }
    QMutexLocker locker(&logMutex);
    m_handlerInstalled = false;
    --installedHandlerCount;
    if (installedHandlerCount > 0) {
        return;
    }
    qInstallMessageHandler(previousMessageHandler);
    previousMessageHandler = nullptr;
    if (activeLog) {
        activeLog->flush();
        activeLog->close();
        activeLog.reset();
    }
}
