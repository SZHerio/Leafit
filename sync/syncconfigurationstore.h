#pragma once

#include <QDateTime>
#include <QSettings>
#include <QVariantMap>

class SyncConfigurationStore final
{
public:
    explicit SyncConfigurationStore(const QString &settingsFilePath = {});

    QString rootPath() const;
    void setRootPath(const QString &rootPath);

    QString deviceId();
    QVariantMap baseProfile() const;
    void setBaseProfile(const QVariantMap &profile);

    QDateTime lastSyncedAt() const;
    void setLastSyncedAt(const QDateTime &dateTime);

    QString settingsFilePath() const;
    static QString defaultSettingsFilePath();

private:
    mutable QSettings m_settings;
};
