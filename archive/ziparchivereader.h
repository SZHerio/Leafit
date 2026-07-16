#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

class ZipArchiveReader final
{
public:
    explicit ZipArchiveReader(const QString &archivePath);
    ~ZipArchiveReader();

    ZipArchiveReader(const ZipArchiveReader &) = delete;
    ZipArchiveReader &operator=(const ZipArchiveReader &) = delete;

    bool isOpen() const;
    QString errorString() const;
    QStringList filePaths() const;
    QByteArray fileData(const QString &path) const;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
