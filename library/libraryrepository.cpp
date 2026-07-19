#include "libraryrepository.h"

#include "bookmetadataservice.h"
#include "../storage/documentstoragekey.h"
#include "../storage/localstatestore.h"

#include <QFileInfo>

LibraryRepository::LibraryRepository(LocalStateStore *store,
                                     BookMetadataService *metadataService,
                                     QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_metadataService(metadataService)
{
    Q_ASSERT(m_store);
    Q_ASSERT(m_metadataService);

    connect(m_store, &LocalStateStore::libraryChanged, this, &LibraryRepository::changed);
    connect(m_store,
            &LocalStateStore::documentProgressChanged,
            this,
            &LibraryRepository::documentProgressChanged);
}

QVector<LibraryBook> LibraryRepository::books()
{
    QVector<LibraryBook> books = m_store->libraryBooks();
    for (LibraryBook &book : books) {
        if (!book.fileAvailable) {
            continue;
        }

        const QString fingerprint = m_metadataService->fingerprint(book.sourceUrl);
        const bool coverMissing = !book.coverUrl.isEmpty()
                                  && book.coverUrl.isLocalFile()
                                  && !QFileInfo::exists(book.coverUrl.toLocalFile());
        if (fingerprint.isEmpty()
            || (fingerprint == book.metadataFingerprint && !coverMissing)) {
            continue;
        }

        const BookMetadata metadata = m_metadataService->inspect(book.sourceUrl);
        if (metadata.hasDocumentTitle || book.title.isEmpty()) {
            book.title = metadata.title;
        }
        if (metadata.hasDocumentAuthor || book.author.isEmpty()) {
            book.author = metadata.author;
        }
        if (!metadata.formatName.isEmpty()) {
            book.formatName = metadata.formatName;
        }
        book.metadataFingerprint = metadata.fingerprint;
        book.coverUrl = metadata.coverUrl;
        m_store->updateBookMetadata(book.sourceUrl,
                                    book.title,
                                    book.author,
                                    book.formatName,
                                    book.coverUrl,
                                    book.metadataFingerprint);
    }
    return books;
}

QString LibraryRepository::sortMode() const
{
    return m_store->librarySortMode();
}

QString LibraryRepository::viewMode() const
{
    return m_store->libraryViewMode();
}

void LibraryRepository::setSortMode(const QString &sortMode)
{
    m_store->setLibrarySortMode(sortMode);
}

void LibraryRepository::setViewMode(const QString &viewMode)
{
    m_store->setLibraryViewMode(viewMode);
}

void LibraryRepository::recordBookOpened(const QUrl &sourceUrl,
                                         const QString &title,
                                         const QString &author,
                                         const QString &formatName)
{
    m_store->recordBookOpened(sourceUrl, title, author, formatName);
}

bool LibraryRepository::registerBook(const QUrl &sourceUrl,
                                     const QString &collectionPath,
                                     bool restoreExcluded)
{
    if (!m_metadataService->supports(sourceUrl)) {
        return false;
    }
    if (m_store->hasLibraryRecord(sourceUrl)) {
        if (m_store->containsLibraryBook(sourceUrl)) {
            m_store->setBookCollection(sourceUrl, collectionPath);
        } else if (restoreExcluded) {
            const BookMetadata metadata = m_metadataService->inspect(sourceUrl);
            m_store->recordBookOpened(sourceUrl,
                                      metadata.title,
                                      metadata.author,
                                      metadata.formatName);
            m_store->setBookCollection(sourceUrl, collectionPath);
            m_store->updateBookMetadata(sourceUrl,
                                        metadata.title,
                                        metadata.author,
                                        metadata.formatName,
                                        metadata.coverUrl,
                                        metadata.fingerprint);
            return true;
        }
        return false;
    }

    const BookMetadata metadata = m_metadataService->inspect(sourceUrl);
    m_store->registerLibraryBook(sourceUrl,
                                 metadata.title,
                                 metadata.author,
                                 metadata.formatName,
                                 metadata.coverUrl,
                                 metadata.fingerprint,
                                 collectionPath);
    return true;
}

bool LibraryRepository::supports(const QUrl &sourceUrl) const
{
    return m_metadataService->supports(sourceUrl);
}

void LibraryRepository::removeBook(const QUrl &sourceUrl)
{
    m_store->removeFromLibrary(sourceUrl);
}

bool LibraryRepository::relinkBook(const QUrl &oldSourceUrl,
                                   const QUrl &newSourceUrl,
                                   QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    };

    if (oldSourceUrl.isEmpty()) {
        return fail(tr("The missing library item is no longer available."));
    }
    if (!newSourceUrl.isLocalFile()) {
        return fail(tr("Choose a local book file."));
    }

    const QFileInfo fileInfo(newSourceUrl.toLocalFile());
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return fail(tr("The selected file does not exist."));
    }

    const QUrl normalizedUrl = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
    if (!m_metadataService->supports(normalizedUrl)) {
        return fail(tr("The selected file format is not supported."));
    }
    if (DocumentStorageKey::id(oldSourceUrl) != DocumentStorageKey::id(normalizedUrl)
        && m_store->containsLibraryBook(normalizedUrl)) {
        return fail(tr("This book is already in your library."));
    }
    if (!m_store->relinkDocument(oldSourceUrl, normalizedUrl)) {
        return fail(tr("Could not update the library item."));
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}
