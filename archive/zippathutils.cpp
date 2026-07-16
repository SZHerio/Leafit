#include "zippathutils.h"

#include "ziparchivereader.h"

#include <QUrl>

namespace ZipPathUtils {

QString clean(QString path)
{
    path.replace(u'\\', u'/');
    QStringList cleanParts;
    for (const QString &part : path.split(u'/', Qt::SkipEmptyParts)) {
        if (part == QLatin1String(".")) {
            continue;
        }
        if (part == QLatin1String("..")) {
            if (!cleanParts.isEmpty()) {
                cleanParts.removeLast();
            }
            continue;
        }
        cleanParts.append(part);
    }
    return cleanParts.join(u'/');
}

QString directory(const QString &path)
{
    const qsizetype separator = path.lastIndexOf(u'/');
    return separator < 0 ? QString() : path.left(separator);
}

QString resolve(const QString &basePath, const QString &href)
{
    if (href.startsWith(u'/')) {
        return clean(href.mid(1));
    }
    return basePath.isEmpty() ? clean(href) : clean(basePath + u'/' + href);
}

QByteArray fileData(const ZipArchiveReader &zip, const QString &path)
{
    QByteArray data = zip.fileData(path);
    if (!data.isEmpty()) {
        return data;
    }

    const QString decodedPath = clean(QUrl::fromPercentEncoding(path.toUtf8()));
    return decodedPath == path ? QByteArray() : zip.fileData(decodedPath);
}

QString firstFileWithSuffix(const ZipArchiveReader &zip, const QString &suffix)
{
    for (const QString &path : zip.filePaths()) {
        if (path.endsWith(suffix, Qt::CaseInsensitive)) {
            return path;
        }
    }
    return {};
}

} // namespace ZipPathUtils
