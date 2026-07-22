#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class DiagnosticService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString applicationVersion READ applicationVersion CONSTANT)
    Q_PROPERTY(QUrl logDirectoryUrl READ logDirectoryUrl CONSTANT)
    Q_PROPERTY(QUrl logFileUrl READ logFileUrl CONSTANT)
    Q_PROPERTY(QString supportSummary READ supportSummary CONSTANT)

public:
    explicit DiagnosticService(const QString &logDirectory = {},
                               QObject *parent = nullptr);
    ~DiagnosticService() override;

    DiagnosticService(const DiagnosticService &) = delete;
    DiagnosticService &operator=(const DiagnosticService &) = delete;

    static QString defaultLogDirectory();

    QString applicationVersion() const;
    QUrl logDirectoryUrl() const;
    QUrl logFileUrl() const;
    QString supportSummary() const;

    Q_INVOKABLE bool openLogDirectory();
    Q_INVOKABLE bool exportCurrentLog(const QUrl &destinationUrl);

signals:
    void operationFailed(const QString &errorMessage);

private:
    void installMessageHandler();
    void uninstallMessageHandler();

    QString m_logDirectory;
    QString m_logFilePath;
    bool m_handlerInstalled = false;
};
