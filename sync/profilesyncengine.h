#pragma once

#include <QDateTime>
#include <QStringList>
#include <QVariantMap>

class ProfileStorage;

struct ProfileSyncResult final
{
    bool success = false;
    bool localProfileChanged = false;
    bool remoteProfileChanged = false;
    QVariantMap mergedProfile;
    QStringList conflictKeys;
    QString conflictFilePath;
    QString errorMessage;
    QDateTime syncedAt;
};

class ProfileSyncEngine final
{
public:
    explicit ProfileSyncEngine(ProfileStorage *storage);

    ProfileSyncResult synchronize(const QString &libraryRootPath,
                                  const QVariantMap &baseProfile,
                                  const QString &deviceId);

    static QVariantMap mergeProfiles(const QVariantMap &baseProfile,
                                     const QVariantMap &localProfile,
                                     const QVariantMap &remoteProfile,
                                     QStringList *conflictKeys = nullptr);
    static QString profileFilePath(const QString &libraryRootPath);

private:
    ProfileStorage *m_storage = nullptr;
};
