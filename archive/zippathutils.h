#pragma once

#include <QByteArray>
#include <QString>

class ZipArchiveReader;

namespace ZipPathUtils {

QString clean(QString path);
QString directory(const QString &path);
QString resolve(const QString &basePath, const QString &href);
QByteArray fileData(const ZipArchiveReader &zip, const QString &path);
QString firstFileWithSuffix(const ZipArchiveReader &zip, const QString &suffix);

} // namespace ZipPathUtils
