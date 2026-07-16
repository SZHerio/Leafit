#include "ziparchivereader.h"

#include <QFile>

#include <limits>

#include <miniz.h>

struct ZipArchiveReader::State final
{
    explicit State(const QString &archivePath)
        : file(archivePath)
    {
    }

    QFile file;
    mz_zip_archive archive{};
    bool open = false;
    QString error;
};

namespace {

constexpr mz_uint64 maximumExtractedFileSize = 256ULL * 1024ULL * 1024ULL;

size_t readArchive(void *opaque, mz_uint64 offset, void *buffer, size_t byteCount)
{
    auto *file = static_cast<QFile *>(opaque);
    if (offset > static_cast<mz_uint64>(std::numeric_limits<qint64>::max())
        || byteCount > static_cast<size_t>(std::numeric_limits<qint64>::max())
        || !file->seek(static_cast<qint64>(offset))) {
        return 0;
    }

    const qint64 bytesRead = file->read(static_cast<char *>(buffer),
                                        static_cast<qint64>(byteCount));
    return bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0;
}

QString archiveError(mz_zip_archive *archive)
{
    return QString::fromLatin1(mz_zip_get_error_string(mz_zip_get_last_error(archive)));
}

} // namespace

ZipArchiveReader::ZipArchiveReader(const QString &archivePath)
    : m_state(std::make_unique<State>(archivePath))
{
    if (!m_state->file.open(QIODevice::ReadOnly)) {
        m_state->error = m_state->file.errorString();
        return;
    }

    m_state->archive.m_pRead = readArchive;
    m_state->archive.m_pIO_opaque = &m_state->file;
    if (!mz_zip_reader_init(&m_state->archive,
                            static_cast<mz_uint64>(m_state->file.size()),
                            0)) {
        m_state->error = archiveError(&m_state->archive);
        return;
    }

    m_state->open = true;
}

ZipArchiveReader::~ZipArchiveReader()
{
    if (m_state->open) {
        mz_zip_reader_end(&m_state->archive);
    }
}

bool ZipArchiveReader::isOpen() const
{
    return m_state->open;
}

QString ZipArchiveReader::errorString() const
{
    return m_state->error;
}

QStringList ZipArchiveReader::filePaths() const
{
    QStringList paths;
    if (!m_state->open) {
        return paths;
    }

    const mz_uint fileCount = mz_zip_reader_get_num_files(&m_state->archive);
    paths.reserve(static_cast<qsizetype>(fileCount));
    for (mz_uint index = 0; index < fileCount; ++index) {
        mz_zip_archive_file_stat fileInfo{};
        if (mz_zip_reader_file_stat(&m_state->archive, index, &fileInfo)
            && !fileInfo.m_is_directory) {
            paths.append(QString::fromUtf8(fileInfo.m_filename));
        }
    }

    return paths;
}

QByteArray ZipArchiveReader::fileData(const QString &path) const
{
    if (!m_state->open) {
        return {};
    }

    const QByteArray encodedPath = path.toUtf8();
    const int fileIndex = mz_zip_reader_locate_file(&m_state->archive,
                                                    encodedPath.constData(),
                                                    nullptr,
                                                    MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (fileIndex < 0) {
        return {};
    }

    mz_zip_archive_file_stat fileInfo{};
    if (!mz_zip_reader_file_stat(&m_state->archive,
                                 static_cast<mz_uint>(fileIndex),
                                 &fileInfo)
        || fileInfo.m_is_directory
        || fileInfo.m_is_encrypted
        || !fileInfo.m_is_supported
        || fileInfo.m_uncomp_size > maximumExtractedFileSize
        || fileInfo.m_uncomp_size
               > static_cast<mz_uint64>(std::numeric_limits<qsizetype>::max())) {
        return {};
    }

    size_t dataSize = 0;
    void *data = mz_zip_reader_extract_to_heap(&m_state->archive,
                                               static_cast<mz_uint>(fileIndex),
                                               &dataSize,
                                               0);
    if (!data) {
        return {};
    }

    const QByteArray result(static_cast<const char *>(data),
                            static_cast<qsizetype>(dataSize));
    mz_free(data);
    return result;
}
