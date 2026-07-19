#pragma once

#include <QVariantMap>

class QUrl;

class PortableProfileMapper final
{
public:
    static QVariantMap toPortable(const QVariantMap &localProfile,
                                  const QString &libraryRootPath);
    static QVariantMap applyPortable(const QVariantMap &localProfile,
                                     const QVariantMap &portableProfile,
                                     const QString &libraryRootPath);

    static bool isManagedBook(const QUrl &bookUrl, const QString &libraryRootPath);
    static QString relativeBookPath(const QUrl &bookUrl,
                                    const QString &libraryRootPath);
};
