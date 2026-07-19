#include "syncconfigurationstore.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QUuid>

namespace {

const QString rootPathKey = QStringLiteral("onedrive/rootPath");
const QString deviceIdKey = QStringLiteral("onedrive/deviceId");
const QString baseProfileKey = QStringLiteral("onedrive/baseProfile");
const QString lastSyncedAtKey = QStringLiteral("onedrive/lastSyncedAt");

} // namespace

SyncConfigurationStore::SyncConfigurationStore(const QString &settingsFilePath)
    : m_settings(settingsFilePath.isEmpty() ? defaultSettingsFilePath() : settingsFilePath,
                 QSettings::IniFormat)
{
}

QString SyncConfigurationStore::rootPath() const
{
    return m_settings.value(rootPathKey).toString();
}

void SyncConfigurationStore::setRootPath(const QString &rootPath)
{
    const QString cleanPath = rootPath.isEmpty()
                                  ? QString()
                                  : QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    if (this->rootPath() == cleanPath) {
        return;
    }

    m_settings.setValue(rootPathKey, cleanPath);
    m_settings.remove(baseProfileKey);
    m_settings.remove(lastSyncedAtKey);
    m_settings.sync();
}

QString SyncConfigurationStore::deviceId()
{
    QString id = m_settings.value(deviceIdKey).toString();
    if (!id.isEmpty()) {
        return id;
    }
    id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_settings.setValue(deviceIdKey, id);
    m_settings.sync();
    return id;
}

QVariantMap SyncConfigurationStore::baseProfile() const
{
    const QByteArray data = m_settings.value(baseProfileKey).toByteArray();
    if (data.isEmpty()) {
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }
    return document.object().toVariantMap();
}

void SyncConfigurationStore::setBaseProfile(const QVariantMap &profile)
{
    m_settings.setValue(baseProfileKey,
                        QJsonDocument(QJsonObject::fromVariantMap(profile))
                            .toJson(QJsonDocument::Compact));
    m_settings.sync();
}

QDateTime SyncConfigurationStore::lastSyncedAt() const
{
    return QDateTime::fromString(m_settings.value(lastSyncedAtKey).toString(),
                                 Qt::ISODateWithMs);
}

void SyncConfigurationStore::setLastSyncedAt(const QDateTime &dateTime)
{
    m_settings.setValue(lastSyncedAtKey, dateTime.toUTC().toString(Qt::ISODateWithMs));
    m_settings.sync();
}

QString SyncConfigurationStore::settingsFilePath() const
{
    return m_settings.fileName();
}

QString SyncConfigurationStore::defaultSettingsFilePath()
{
    const QString overriddenPath = qEnvironmentVariable("SZHBOOKS_SYNC_SETTINGS_FILE");
    if (!overriddenPath.isEmpty()) {
        const QFileInfo fileInfo(overriddenPath);
        QDir().mkpath(fileInfo.absolutePath());
        return fileInfo.absoluteFilePath();
    }

    QString directory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (directory.isEmpty()) {
        directory = QDir::home().filePath(QStringLiteral(".szhbooks"));
    }
    QDir().mkpath(directory);
    return QDir(directory).filePath(QStringLiteral("sync.ini"));
}
