#include "profilesyncengine.h"

#include "portableprofilemapper.h"
#include "../storage/profilestorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {

constexpr int profileSchemaVersion = 1;
constexpr qint64 maximumProfileSize = 16 * 1024 * 1024;
const QString metadataDirectoryName = QStringLiteral(".szhbooks");
const QString profileFileName = QStringLiteral("profile.json");

struct RemoteProfile final
{
    bool exists = false;
    bool valid = false;
    QVariantMap settings;
    QString errorMessage;
};

QByteArray jsonData(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

bool writeAtomically(const QString &path, const QByteArray &data, QString *errorMessage)
{
    const QFileInfo fileInfo(path);
    if (!QDir().mkpath(fileInfo.absolutePath())) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate(
                "ProfileSyncEngine", "Could not create the synchronization metadata folder.");
        }
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(data) != data.size()
        || !file.commit()) {
        if (errorMessage) {
            *errorMessage = QCoreApplication::translate(
                "ProfileSyncEngine", "Could not update the synchronized book profile.");
        }
        return false;
    }
    return true;
}

RemoteProfile readRemoteProfile(const QString &path)
{
    RemoteProfile result;
    const QFileInfo fileInfo(path);
    result.exists = fileInfo.exists();
    if (!result.exists) {
        result.valid = true;
        return result;
    }
    if (!fileInfo.isFile() || fileInfo.size() <= 0 || fileInfo.size() > maximumProfileSize) {
        result.errorMessage = QCoreApplication::translate(
            "ProfileSyncEngine", "The synchronized profile is empty or too large.");
        return result;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = QCoreApplication::translate(
            "ProfileSyncEngine", "Could not read the synchronized profile.");
        return result;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.errorMessage = QCoreApplication::translate(
            "ProfileSyncEngine", "The synchronized profile is damaged.");
        return result;
    }

    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("product")).toString() != QLatin1String("SZHBooks")
        || object.value(QStringLiteral("schemaVersion")).toInt() != profileSchemaVersion
        || !object.value(QStringLiteral("settings")).isObject()) {
        result.errorMessage = QCoreApplication::translate(
            "ProfileSyncEngine", "The synchronized profile version is not supported.");
        return result;
    }

    result.settings = object.value(QStringLiteral("settings")).toObject().toVariantMap();
    result.valid = true;
    return result;
}

QJsonObject profileObject(const QVariantMap &settings,
                          const QString &deviceId,
                          const QDateTime &updatedAt)
{
    return {
        {QStringLiteral("product"), QStringLiteral("SZHBooks")},
        {QStringLiteral("schemaVersion"), profileSchemaVersion},
        {QStringLiteral("updatedAt"), updatedAt.toUTC().toString(Qt::ISODateWithMs)},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("settings"), QJsonObject::fromVariantMap(settings)}
    };
}

bool equivalent(const QVariantMap &left, const QVariantMap &right)
{
    return jsonData(QJsonObject::fromVariantMap(left))
           == jsonData(QJsonObject::fromVariantMap(right));
}

struct ProfileValue final
{
    bool present = false;
    QVariant value;
};

ProfileValue profileValue(const QVariantMap &profile, const QString &key)
{
    return {profile.contains(key), profile.value(key)};
}

bool equivalent(const ProfileValue &left, const ProfileValue &right)
{
    if (left.present != right.present) {
        return false;
    }
    return !left.present
           || QJsonValue::fromVariant(left.value) == QJsonValue::fromVariant(right.value);
}

void insertValue(QVariantMap *profile, const QString &key, const ProfileValue &value)
{
    if (value.present) {
        profile->insert(key, value.value);
    }
}

QString safeDeviceName(QString deviceId)
{
    deviceId.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")));
    return deviceId.left(48).isEmpty() ? QStringLiteral("device") : deviceId.left(48);
}

bool writeConflictFile(const QString &libraryRootPath,
                       const QString &deviceId,
                       const QStringList &conflictKeys,
                       const QVariantMap &baseProfile,
                       const QVariantMap &localProfile,
                       const QVariantMap &remoteProfile,
                       QString *filePath,
                       QString *errorMessage)
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QString conflictsDirectory = QDir(libraryRootPath).filePath(
        metadataDirectoryName + QStringLiteral("/conflicts"));
    const QString name = QStringLiteral("profile-%1-%2.json")
                             .arg(now.toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")),
                                  safeDeviceName(deviceId));
    const QString path = QDir(conflictsDirectory).filePath(name);
    const QJsonObject object{
        {QStringLiteral("product"), QStringLiteral("SZHBooks")},
        {QStringLiteral("schemaVersion"), profileSchemaVersion},
        {QStringLiteral("createdAt"), now.toString(Qt::ISODateWithMs)},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("conflictKeys"), QJsonArray::fromStringList(conflictKeys)},
        {QStringLiteral("baseSettings"), QJsonObject::fromVariantMap(baseProfile)},
        {QStringLiteral("localSettings"), QJsonObject::fromVariantMap(localProfile)},
        {QStringLiteral("remoteSettings"), QJsonObject::fromVariantMap(remoteProfile)}
    };
    if (!writeAtomically(path, jsonData(object), errorMessage)) {
        return false;
    }
    if (filePath) {
        *filePath = path;
    }
    return true;
}

} // namespace

ProfileSyncEngine::ProfileSyncEngine(ProfileStorage *storage)
    : m_storage(storage)
{
    Q_ASSERT(m_storage);
}

ProfileSyncResult ProfileSyncEngine::synchronize(const QString &libraryRootPath,
                                                 const QVariantMap &baseProfile,
                                                 const QString &deviceId)
{
    ProfileSyncResult result;
    const QString rootPath = QDir::cleanPath(QFileInfo(libraryRootPath).absoluteFilePath());
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.errorMessage = QCoreApplication::translate(
            "ProfileSyncEngine", "The selected synchronization folder is unavailable.");
        return result;
    }

    const QVariantMap localProfile = PortableProfileMapper::toPortable(
        m_storage->profileValues(), rootPath);
    const QString remotePath = profileFilePath(rootPath);
    const RemoteProfile remote = readRemoteProfile(remotePath);
    if (!remote.valid) {
        result.errorMessage = remote.errorMessage;
        return result;
    }

    result.mergedProfile = remote.exists
                               ? mergeProfiles(baseProfile,
                                               localProfile,
                                               remote.settings,
                                               &result.conflictKeys)
                               : localProfile;
    if (!result.conflictKeys.isEmpty()
        && !writeConflictFile(rootPath,
                              deviceId,
                              result.conflictKeys,
                              baseProfile,
                              localProfile,
                              remote.settings,
                              &result.conflictFilePath,
                              &result.errorMessage)) {
        return result;
    }

    result.syncedAt = QDateTime::currentDateTimeUtc();
    if (!remote.exists || !equivalent(remote.settings, result.mergedProfile)) {
        if (!writeAtomically(remotePath,
                             jsonData(profileObject(result.mergedProfile,
                                                    deviceId,
                                                    result.syncedAt)),
                             &result.errorMessage)) {
            return result;
        }
        result.remoteProfileChanged = true;
    }

    if (!equivalent(localProfile, result.mergedProfile)) {
        const QVariantMap replacement = PortableProfileMapper::applyPortable(
            m_storage->profileValues(), result.mergedProfile, rootPath);
        QString storageError;
        if (!m_storage->replaceProfileValues(replacement, &storageError)) {
            result.errorMessage = QCoreApplication::translate(
                                      "ProfileSyncEngine",
                                      "Could not apply the synchronized profile: %1")
                                      .arg(storageError);
            return result;
        }
        result.localProfileChanged = true;
    }

    result.success = true;
    return result;
}

QVariantMap ProfileSyncEngine::mergeProfiles(const QVariantMap &baseProfile,
                                             const QVariantMap &localProfile,
                                             const QVariantMap &remoteProfile,
                                             QStringList *conflictKeys)
{
    QSet<QString> keys;
    for (const QString &key : baseProfile.keys()) {
        keys.insert(key);
    }
    for (const QString &key : localProfile.keys()) {
        keys.insert(key);
    }
    for (const QString &key : remoteProfile.keys()) {
        keys.insert(key);
    }

    QStringList sortedKeys(keys.cbegin(), keys.cend());
    std::sort(sortedKeys.begin(), sortedKeys.end());
    QStringList conflicts;
    QVariantMap merged;
    for (const QString &key : sortedKeys) {
        const ProfileValue base = profileValue(baseProfile, key);
        const ProfileValue local = profileValue(localProfile, key);
        const ProfileValue remote = profileValue(remoteProfile, key);

        if (equivalent(local, remote)) {
            insertValue(&merged, key, local);
        } else if (equivalent(local, base)) {
            insertValue(&merged, key, remote);
        } else if (equivalent(remote, base)) {
            insertValue(&merged, key, local);
        } else {
            // Keep the active device's value and preserve the remote snapshot separately.
            insertValue(&merged, key, local);
            conflicts.append(key);
        }
    }

    if (conflictKeys) {
        *conflictKeys = conflicts;
    }
    return merged;
}

QString ProfileSyncEngine::profileFilePath(const QString &libraryRootPath)
{
    return QDir(libraryRootPath).filePath(metadataDirectoryName + u'/' + profileFileName);
}
