#include "portableprofilemapper.h"

#include "../storage/documentstoragekey.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QUrl>

namespace {

const QString documentsPrefix = QStringLiteral("documents/");
const QString annotationsPrefix = QStringLiteral("annotations/");
const QString sourceUrlSuffix = QStringLiteral("/sourceUrl");
const QString portableScheme = QStringLiteral("szhbooks");
const QString portableHost = QStringLiteral("library");

QString cleanRootPath(const QString &rootPath)
{
    return QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
}

bool isWithinRoot(const QString &path, const QString &rootPath)
{
    if (path.isEmpty() || rootPath.isEmpty()) {
        return false;
    }

    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString cleanRoot = cleanRootPath(rootPath);
    if (cleanPath.compare(cleanRoot,
#ifdef Q_OS_WIN
                          Qt::CaseInsensitive
#else
                          Qt::CaseSensitive
#endif
                          ) == 0) {
        return false;
    }

    const QString relativePath = QDir(cleanRoot).relativeFilePath(cleanPath);
    return !QDir::isAbsolutePath(relativePath)
           && relativePath != QLatin1String("..")
           && !relativePath.startsWith(QStringLiteral("../"))
           && !relativePath.startsWith(QStringLiteral("..\\"));
}

QUrl urlFromValue(const QVariant &value)
{
    const QUrl converted = value.toUrl();
    return converted.isEmpty() ? QUrl(value.toString()) : converted;
}

QString portableUrlForPath(QString relativePath)
{
    relativePath.replace(u'\\', u'/');
    QUrl url;
    url.setScheme(portableScheme);
    url.setHost(portableHost);
    url.setPath(u'/' + relativePath);
    return url.toString(QUrl::FullyEncoded);
}

QString pathFromPortableUrl(const QVariant &value)
{
    const QUrl url = urlFromValue(value);
    if (url.scheme() != portableScheme || url.host() != portableHost) {
        return {};
    }

    QString path = url.path(QUrl::FullyDecoded);
    if (path.startsWith(u'/')) {
        path.remove(0, 1);
    }
    path.replace(u'\\', u'/');
    const QString cleanPath = QDir::cleanPath(path);
    if (cleanPath.isEmpty()
        || cleanPath == QLatin1String(".")
        || QDir::isAbsolutePath(cleanPath)
        || cleanPath == QLatin1String("..")
        || cleanPath.startsWith(QStringLiteral("../"))) {
        return {};
    }
    return QString(cleanPath).replace(u'\\', u'/');
}

QString portableDocumentId(const QString &portableUrl)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(portableUrl.toUtf8(), QCryptographicHash::Sha256).toHex());
}

QHash<QString, QVariant> sourceUrlsByGroup(const QVariantMap &profile,
                                           const QString &prefix)
{
    QHash<QString, QVariant> sourceUrls;
    for (auto entry = profile.cbegin(); entry != profile.cend(); ++entry) {
        if (!entry.key().startsWith(prefix)
            || !entry.key().endsWith(sourceUrlSuffix)) {
            continue;
        }
        const QString tail = entry.key().mid(prefix.size());
        const qsizetype separator = tail.indexOf(u'/');
        if (separator > 0 && tail.mid(separator) == sourceUrlSuffix) {
            sourceUrls.insert(tail.left(separator), entry.value());
        }
    }
    return sourceUrls;
}

bool isGroupKey(const QString &key, const QString &prefix, const QString &groupId)
{
    return key.startsWith(prefix + groupId + u'/');
}

bool isCacheOnlyDocumentValue(const QString &key)
{
    return key.endsWith(QStringLiteral("/coverUrl"))
           || key.endsWith(QStringLiteral("/metadataFingerprint"));
}

void appendPortableGroups(QVariantMap *portable,
                          const QVariantMap &local,
                          const QString &prefix,
                          const QString &rootPath)
{
    const QHash<QString, QVariant> sourceUrls = sourceUrlsByGroup(local, prefix);
    for (auto source = sourceUrls.cbegin(); source != sourceUrls.cend(); ++source) {
        const QString relativePath = PortableProfileMapper::relativeBookPath(
            urlFromValue(source.value()), rootPath);
        if (relativePath.isEmpty()) {
            continue;
        }

        const QString portableUrl = portableUrlForPath(relativePath);
        const QString portableId = portableDocumentId(portableUrl);
        for (auto entry = local.cbegin(); entry != local.cend(); ++entry) {
            if (!isGroupKey(entry.key(), prefix, source.key())
                || isCacheOnlyDocumentValue(entry.key())) {
                continue;
            }
            const QString suffix = entry.key().mid(prefix.size() + source.key().size());
            portable->insert(prefix + portableId + suffix,
                             suffix == sourceUrlSuffix ? QVariant(portableUrl)
                                                       : entry.value());
        }
    }
}

QSet<QString> managedGroupIds(const QVariantMap &profile,
                              const QString &prefix,
                              const QString &rootPath)
{
    QSet<QString> ids;
    const QHash<QString, QVariant> sourceUrls = sourceUrlsByGroup(profile, prefix);
    for (auto source = sourceUrls.cbegin(); source != sourceUrls.cend(); ++source) {
        if (PortableProfileMapper::isManagedBook(urlFromValue(source.value()), rootPath)) {
            ids.insert(source.key());
        }
    }
    return ids;
}

void appendLocalGroups(QVariantMap *local,
                       const QVariantMap &portable,
                       const QString &prefix,
                       const QString &rootPath)
{
    const QHash<QString, QVariant> sourceUrls = sourceUrlsByGroup(portable, prefix);
    for (auto source = sourceUrls.cbegin(); source != sourceUrls.cend(); ++source) {
        const QString relativePath = pathFromPortableUrl(source.value());
        if (relativePath.isEmpty()) {
            continue;
        }

        const QUrl localUrl = QUrl::fromLocalFile(
            QDir(rootPath).absoluteFilePath(relativePath));
        const QString localId = DocumentStorageKey::id(localUrl);
        for (auto entry = portable.cbegin(); entry != portable.cend(); ++entry) {
            if (!isGroupKey(entry.key(), prefix, source.key())) {
                continue;
            }
            const QString suffix = entry.key().mid(prefix.size() + source.key().size());
            local->insert(prefix + localId + suffix,
                          suffix == sourceUrlSuffix
                              ? QVariant(localUrl.toString(QUrl::FullyEncoded))
                              : entry.value());
        }
    }
}

bool isDocumentKey(const QString &key)
{
    return key.startsWith(documentsPrefix) || key.startsWith(annotationsPrefix);
}

} // namespace

QVariantMap PortableProfileMapper::toPortable(const QVariantMap &localProfile,
                                              const QString &libraryRootPath)
{
    QVariantMap portable;
    const QString rootPath = cleanRootPath(libraryRootPath);
    for (auto entry = localProfile.cbegin(); entry != localProfile.cend(); ++entry) {
        if (isDocumentKey(entry.key())) {
            continue;
        }
        if (entry.key() == QLatin1String("session/lastBookUrl")) {
            const QString relativePath = relativeBookPath(urlFromValue(entry.value()), rootPath);
            if (!relativePath.isEmpty()) {
                portable.insert(entry.key(), portableUrlForPath(relativePath));
            }
            continue;
        }
        portable.insert(entry.key(), entry.value());
    }

    appendPortableGroups(&portable, localProfile, documentsPrefix, rootPath);
    appendPortableGroups(&portable, localProfile, annotationsPrefix, rootPath);
    return portable;
}

QVariantMap PortableProfileMapper::applyPortable(const QVariantMap &localProfile,
                                                 const QVariantMap &portableProfile,
                                                 const QString &libraryRootPath)
{
    const QString rootPath = cleanRootPath(libraryRootPath);
    const QSet<QString> managedDocumentIds = managedGroupIds(
        localProfile, documentsPrefix, rootPath);
    const QSet<QString> managedAnnotationIds = managedGroupIds(
        localProfile, annotationsPrefix, rootPath);

    QVariantMap result;
    for (auto entry = localProfile.cbegin(); entry != localProfile.cend(); ++entry) {
        if (!isDocumentKey(entry.key())) {
            if (entry.key() == QLatin1String("session/lastBookUrl")
                && !isManagedBook(urlFromValue(entry.value()), rootPath)) {
                result.insert(entry.key(), entry.value());
            }
            continue;
        }

        const QString prefix = entry.key().startsWith(documentsPrefix)
                                   ? documentsPrefix
                                   : annotationsPrefix;
        const QSet<QString> &managedIds = prefix == documentsPrefix
                                              ? managedDocumentIds
                                              : managedAnnotationIds;
        bool managed = false;
        for (const QString &id : managedIds) {
            if (isGroupKey(entry.key(), prefix, id)) {
                managed = true;
                break;
            }
        }
        if (!managed) {
            result.insert(entry.key(), entry.value());
        }
    }

    for (auto entry = portableProfile.cbegin(); entry != portableProfile.cend(); ++entry) {
        if (isDocumentKey(entry.key())) {
            continue;
        }
        if (entry.key() == QLatin1String("session/lastBookUrl")) {
            const QString relativePath = pathFromPortableUrl(entry.value());
            if (!relativePath.isEmpty()) {
                result.insert(entry.key(),
                              QUrl::fromLocalFile(QDir(rootPath).absoluteFilePath(relativePath))
                                  .toString(QUrl::FullyEncoded));
            }
            continue;
        }
        result.insert(entry.key(), entry.value());
    }

    appendLocalGroups(&result, portableProfile, documentsPrefix, rootPath);
    appendLocalGroups(&result, portableProfile, annotationsPrefix, rootPath);
    return result;
}

bool PortableProfileMapper::isManagedBook(const QUrl &bookUrl,
                                          const QString &libraryRootPath)
{
    return bookUrl.isLocalFile()
           && isWithinRoot(bookUrl.toLocalFile(), libraryRootPath);
}

QString PortableProfileMapper::relativeBookPath(const QUrl &bookUrl,
                                                const QString &libraryRootPath)
{
    if (!isManagedBook(bookUrl, libraryRootPath)) {
        return {};
    }
    QString relativePath = QDir(cleanRootPath(libraryRootPath)).relativeFilePath(
        QFileInfo(bookUrl.toLocalFile()).absoluteFilePath());
    return relativePath.replace(u'\\', u'/');
}
