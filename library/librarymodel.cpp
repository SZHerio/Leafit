#include "librarymodel.h"

#include "../storage/localstatestore.h"

#include <QtGlobal>

#include <utility>

LibraryModel::LibraryModel(LocalStateStore *store, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
{
    Q_ASSERT(m_store);

    connect(m_store, &LocalStateStore::libraryChanged, this, &LibraryModel::refresh);
    connect(m_store,
            &LocalStateStore::documentProgressChanged,
            this,
            &LibraryModel::updateProgress);
    refresh();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleBooks.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleBooks.size()) {
        return {};
    }

    const LibraryBook &book = m_visibleBooks.at(index.row());
    switch (role) {
    case SourceUrlRole:
        return book.sourceUrl;
    case SourcePathRole:
        return book.sourcePath;
    case TitleRole:
        return book.title;
    case FormatNameRole:
        return book.formatName;
    case ProgressRole:
        return book.progress;
    case LastOpenedRole:
        return book.lastOpened;
    case FileAvailableRole:
        return book.fileAvailable;
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {SourceUrlRole, "sourceUrl"},
        {SourcePathRole, "sourcePath"},
        {TitleRole, "title"},
        {FormatNameRole, "formatName"},
        {ProgressRole, "progress"},
        {LastOpenedRole, "lastOpened"},
        {FileAvailableRole, "fileAvailable"}
    };
}

QString LibraryModel::filterText() const
{
    return m_filterText;
}

int LibraryModel::totalCount() const
{
    return m_allBooks.size();
}

QVariantMap LibraryModel::recentBook() const
{
    return m_allBooks.isEmpty() ? QVariantMap() : toVariantMap(m_allBooks.constFirst());
}

void LibraryModel::setFilterText(const QString &filterText)
{
    const QString normalizedFilter = filterText.trimmed();
    if (m_filterText == normalizedFilter) {
        return;
    }

    m_filterText = normalizedFilter;
    emit filterTextChanged();
    rebuildVisibleBooks();
}

void LibraryModel::refresh()
{
    const int previousTotalCount = m_allBooks.size();
    m_allBooks = m_store->libraryBooks();

    rebuildVisibleBooks();
    if (previousTotalCount != m_allBooks.size()) {
        emit totalCountChanged();
    }
    emit recentBookChanged();
}

void LibraryModel::removeBook(int row)
{
    if (row < 0 || row >= m_visibleBooks.size()) {
        return;
    }

    m_store->removeFromLibrary(m_visibleBooks.at(row).sourceUrl);
}

QVariantMap LibraryModel::toVariantMap(const LibraryBook &book)
{
    return {
        {QStringLiteral("sourceUrl"), book.sourceUrl},
        {QStringLiteral("sourcePath"), book.sourcePath},
        {QStringLiteral("title"), book.title},
        {QStringLiteral("formatName"), book.formatName},
        {QStringLiteral("progress"), book.progress},
        {QStringLiteral("lastOpened"), book.lastOpened},
        {QStringLiteral("fileAvailable"), book.fileAvailable}
    };
}

bool LibraryModel::matchesFilter(const LibraryBook &book, const QString &filterText)
{
    if (filterText.isEmpty()) {
        return true;
    }

    return book.title.contains(filterText, Qt::CaseInsensitive)
           || book.formatName.contains(filterText, Qt::CaseInsensitive)
           || book.sourcePath.contains(filterText, Qt::CaseInsensitive);
}

void LibraryModel::rebuildVisibleBooks()
{
    const int previousCount = m_visibleBooks.size();

    beginResetModel();
    m_visibleBooks.clear();
    m_visibleBooks.reserve(m_allBooks.size());
    for (const LibraryBook &book : std::as_const(m_allBooks)) {
        if (matchesFilter(book, m_filterText)) {
            m_visibleBooks.append(book);
        }
    }
    endResetModel();

    if (previousCount != m_visibleBooks.size()) {
        emit countChanged();
    }
}

void LibraryModel::updateProgress(const QUrl &sourceUrl, qreal progress)
{
    progress = qBound(qreal(0), progress, qreal(1));
    bool recentBookUpdated = false;

    for (qsizetype row = 0; row < m_allBooks.size(); ++row) {
        LibraryBook &book = m_allBooks[row];
        if (book.sourceUrl != sourceUrl) {
            continue;
        }

        book.progress = progress;
        recentBookUpdated = row == 0;
        break;
    }

    for (qsizetype row = 0; row < m_visibleBooks.size(); ++row) {
        LibraryBook &book = m_visibleBooks[row];
        if (book.sourceUrl != sourceUrl) {
            continue;
        }

        book.progress = progress;
        const QModelIndex changedIndex = index(row, 0);
        emit dataChanged(changedIndex, changedIndex, {ProgressRole});
        break;
    }

    if (recentBookUpdated) {
        emit recentBookChanged();
    }
}
