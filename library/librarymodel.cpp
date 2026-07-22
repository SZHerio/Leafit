#include "librarymodel.h"

#include "libraryrepository.h"
#include "libraryscanservice.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtGlobal>

#include <algorithm>
#include <utility>

namespace {

constexpr qreal unreadProgressThreshold = 0.001;
constexpr qreal finishedProgressThreshold = 0.995;

int compareText(const QString &left, const QString &right)
{
    return QString::localeAwareCompare(left.trimmed(), right.trimmed());
}

QString selectionKey(const QUrl &sourceUrl)
{
    return sourceUrl.toString(QUrl::FullyEncoded);
}

QStringList normalizedList(const QVariant &value)
{
    QStringList input;
    if (value.metaType() == QMetaType::fromType<QString>()) {
        input = value.toString().split(QRegularExpression(QStringLiteral("[,;]")),
                                       Qt::SkipEmptyParts);
    } else if (value.metaType() == QMetaType::fromType<QStringList>()) {
        input = value.toStringList();
    } else if (value.canConvert<QVariantList>()) {
        for (const QVariant &item : value.toList()) {
            input.append(item.toString());
        }
    }

    QStringList result;
    for (QString item : std::as_const(input)) {
        item = item.trimmed().left(64);
        if (!item.isEmpty() && !result.contains(item, Qt::CaseInsensitive)) {
            result.append(item);
        }
        if (result.size() >= 24) {
            break;
        }
    }
    return result;
}

bool containsValue(const QStringList &values, const QString &needle)
{
    return std::any_of(values.cbegin(), values.cend(), [&needle](const QString &value) {
        return value.compare(needle, Qt::CaseInsensitive) == 0;
    });
}

} // namespace

LibraryModel::LibraryModel(LibraryRepository *repository, QObject *parent)
    : QAbstractListModel(parent)
    , m_repository(repository)
{
    Q_ASSERT(m_repository);
    m_sortMode = m_repository->sortMode();
    m_viewMode = m_repository->viewMode();

    connect(m_repository, &LibraryRepository::changed, this, &LibraryModel::refresh);
    connect(m_repository,
            &LibraryRepository::documentProgressChanged,
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
    case AuthorRole:
        return book.author;
    case SeriesRole:
        return book.series;
    case SeriesNumberRole:
        return book.seriesNumber;
    case GenresRole:
        return book.genres;
    case TagsRole:
        return book.tags;
    case LanguageRole:
        return book.language;
    case PublicationYearRole:
        return book.publicationYear;
    case FormatNameRole:
        return book.formatName;
    case CoverUrlRole:
        return book.coverUrl;
    case ProgressRole:
        return book.progress;
    case LastOpenedRole:
        return book.lastOpened;
    case FileAvailableRole:
        return book.fileAvailable;
    case CollectionPathRole:
        return book.collectionPath;
    case CloudPlaceholderRole:
        return book.cloudPlaceholder;
    case SelectedRole:
        return m_selectedBookKeys.contains(selectionKey(book.sourceUrl));
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
        {AuthorRole, "author"},
        {SeriesRole, "series"},
        {SeriesNumberRole, "seriesNumber"},
        {GenresRole, "genres"},
        {TagsRole, "tags"},
        {LanguageRole, "language"},
        {PublicationYearRole, "publicationYear"},
        {FormatNameRole, "formatName"},
        {CoverUrlRole, "coverUrl"},
        {ProgressRole, "progress"},
        {LastOpenedRole, "lastOpened"},
        {FileAvailableRole, "fileAvailable"},
        {CollectionPathRole, "collectionPath"},
        {CloudPlaceholderRole, "cloudPlaceholder"},
        {SelectedRole, "selected"}
    };
}

QString LibraryModel::filterText() const
{
    return m_filterText;
}

QString LibraryModel::sortMode() const
{
    return m_sortMode;
}

QString LibraryModel::formatFilter() const
{
    return m_formatFilter;
}

QString LibraryModel::progressFilter() const
{
    return m_progressFilter;
}

QString LibraryModel::collectionFilter() const
{
    return m_collectionFilter;
}

QString LibraryModel::genreFilter() const
{
    return m_genreFilter;
}

QString LibraryModel::tagFilter() const
{
    return m_tagFilter;
}

QString LibraryModel::languageFilter() const
{
    return m_languageFilter;
}

QString LibraryModel::viewMode() const
{
    return m_viewMode;
}

QStringList LibraryModel::availableFormats() const
{
    return m_availableFormats;
}

QStringList LibraryModel::availableGenres() const
{
    return m_availableGenres;
}

QStringList LibraryModel::availableTags() const
{
    return m_availableTags;
}

QStringList LibraryModel::availableLanguages() const
{
    return m_availableLanguages;
}

bool LibraryModel::hasActiveFilters() const
{
    return !m_filterText.isEmpty()
           || m_formatFilter != QLatin1String("all")
           || m_progressFilter != QLatin1String("all")
           || m_genreFilter != QLatin1String("all")
           || m_tagFilter != QLatin1String("all")
           || m_languageFilter != QLatin1String("all")
           || !m_collectionFilter.isEmpty();
}

bool LibraryModel::selectionMode() const
{
    return m_selectionMode;
}

int LibraryModel::selectionCount() const
{
    return m_selectedBookKeys.size();
}

QVariantList LibraryModel::selectedBooks() const
{
    QVariantList books;
    for (const LibraryBook &book : m_allBooks) {
        if (m_selectedBookKeys.contains(selectionKey(book.sourceUrl))) {
            books.append(toVariantMap(book));
        }
    }
    return books;
}

QString LibraryModel::errorMessage() const
{
    return m_errorMessage;
}

int LibraryModel::totalCount() const
{
    return m_allBooks.size();
}

QVariantMap LibraryModel::recentBook() const
{
    return m_allBooks.isEmpty() ? QVariantMap() : toVariantMap(m_allBooks.constFirst());
}

bool LibraryModel::scanning() const
{
    return m_scanService && m_scanService->scanning();
}

bool LibraryModel::scanCancellationRequested() const
{
    return m_scanService && m_scanService->cancellationRequested();
}

int LibraryModel::scanCompleted() const
{
    return m_scanService ? m_scanService->completed() : 0;
}

int LibraryModel::scanTotal() const
{
    return m_scanService ? m_scanService->total() : 0;
}

qreal LibraryModel::scanProgress() const
{
    return m_scanService ? m_scanService->progress() : 0;
}

void LibraryModel::setScanService(LibraryScanService *scanService)
{
    if (m_scanService == scanService) {
        return;
    }
    if (m_scanService) {
        disconnect(m_scanService, nullptr, this, nullptr);
    }
    m_scanService = scanService;
    if (m_scanService) {
        connect(m_scanService,
                &LibraryScanService::scanningChanged,
                this,
                &LibraryModel::scanStateChanged);
        connect(m_scanService,
                &LibraryScanService::progressChanged,
                this,
                &LibraryModel::scanProgressChanged);
        connect(m_scanService,
                &LibraryScanService::resultsReady,
                this,
                &LibraryModel::applyScanResults);
        m_scanService->start(m_allBooks);
    }
    emit scanStateChanged();
    emit scanProgressChanged();
}

void LibraryModel::setFilterText(const QString &filterText)
{
    const QString normalizedFilter = filterText.trimmed();
    if (m_filterText == normalizedFilter) {
        return;
    }

    const bool hadActiveFilters = hasActiveFilters();
    m_filterText = normalizedFilter;
    emit filterTextChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setSortMode(const QString &sortMode)
{
    const QString normalizedMode = sortMode == QLatin1String("title")
                                       || sortMode == QLatin1String("author")
                                       || sortMode == QLatin1String("series")
                                       || sortMode == QLatin1String("year")
                                       ? sortMode
                                       : QStringLiteral("recent");
    if (m_sortMode == normalizedMode) {
        return;
    }

    m_sortMode = normalizedMode;
    m_repository->setSortMode(normalizedMode);
    emit sortModeChanged();
    rebuildVisibleBooks();
}

void LibraryModel::setFormatFilter(const QString &formatFilter)
{
    const QString normalizedFilter = formatFilter.trimmed().isEmpty()
                                         ? QStringLiteral("all")
                                         : formatFilter.trimmed();
    if (m_formatFilter.compare(normalizedFilter, Qt::CaseInsensitive) == 0) {
        return;
    }

    const bool hadActiveFilters = hasActiveFilters();
    m_formatFilter = normalizedFilter;
    emit formatFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setProgressFilter(const QString &progressFilter)
{
    const QString normalizedFilter = progressFilter == QLatin1String("unread")
                                             || progressFilter == QLatin1String("reading")
                                             || progressFilter == QLatin1String("finished")
                                         ? progressFilter
                                         : QStringLiteral("all");
    if (m_progressFilter == normalizedFilter) {
        return;
    }

    const bool hadActiveFilters = hasActiveFilters();
    m_progressFilter = normalizedFilter;
    emit progressFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setCollectionFilter(const QString &collectionFilter)
{
    QString normalizedFilter = collectionFilter.trimmed();
    normalizedFilter.replace(u'\\', u'/');
    if (normalizedFilter != QLatin1String(".")) {
        normalizedFilter = QDir::cleanPath(normalizedFilter);
        if (normalizedFilter == QLatin1String(".")) {
            normalizedFilter.clear();
        }
    }
    if (m_collectionFilter == normalizedFilter) {
        return;
    }

    const bool hadActiveFilters = hasActiveFilters();
    m_collectionFilter = normalizedFilter;
    emit collectionFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setGenreFilter(const QString &genreFilter)
{
    const QString normalizedFilter = genreFilter.trimmed().isEmpty()
                                         ? QStringLiteral("all")
                                         : genreFilter.trimmed();
    if (m_genreFilter.compare(normalizedFilter, Qt::CaseInsensitive) == 0) {
        return;
    }
    const bool hadActiveFilters = hasActiveFilters();
    m_genreFilter = normalizedFilter;
    emit genreFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setTagFilter(const QString &tagFilter)
{
    const QString normalizedFilter = tagFilter.trimmed().isEmpty()
                                         ? QStringLiteral("all")
                                         : tagFilter.trimmed();
    if (m_tagFilter.compare(normalizedFilter, Qt::CaseInsensitive) == 0) {
        return;
    }
    const bool hadActiveFilters = hasActiveFilters();
    m_tagFilter = normalizedFilter;
    emit tagFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setLanguageFilter(const QString &languageFilter)
{
    const QString normalizedFilter = languageFilter.trimmed().isEmpty()
                                         ? QStringLiteral("all")
                                         : languageFilter.trimmed();
    if (m_languageFilter.compare(normalizedFilter, Qt::CaseInsensitive) == 0) {
        return;
    }
    const bool hadActiveFilters = hasActiveFilters();
    m_languageFilter = normalizedFilter;
    emit languageFilterChanged();
    rebuildVisibleBooks();
    if (hadActiveFilters != hasActiveFilters()) {
        emit hasActiveFiltersChanged();
    }
}

void LibraryModel::setViewMode(const QString &viewMode)
{
    const QString normalizedMode = viewMode == QLatin1String("list")
                                       ? QStringLiteral("list")
                                       : QStringLiteral("grid");
    if (m_viewMode == normalizedMode) {
        return;
    }

    m_viewMode = normalizedMode;
    m_repository->setViewMode(normalizedMode);
    emit viewModeChanged();
}

void LibraryModel::setSelectionMode(bool selectionMode)
{
    if (m_selectionMode == selectionMode) {
        return;
    }
    m_selectionMode = selectionMode;
    if (!m_selectionMode) {
        clearSelection();
    }
    emit selectionModeChanged();
}

void LibraryModel::refresh()
{
    const int previousTotalCount = m_allBooks.size();
    const QString storedSortMode = m_repository->sortMode();
    const QString storedViewMode = m_repository->viewMode();
    if (m_sortMode != storedSortMode) {
        m_sortMode = storedSortMode;
        emit sortModeChanged();
    }
    if (m_viewMode != storedViewMode) {
        m_viewMode = storedViewMode;
        emit viewModeChanged();
    }
    m_allBooks = m_repository->books();
    rebuildBookRows();

    rebuildAvailableFormats();
    rebuildMetadataFilters();
    pruneSelection();
    rebuildVisibleBooks();
    if (previousTotalCount != m_allBooks.size()) {
        emit totalCountChanged();
    }
    emit recentBookChanged();
    if (m_scanService) {
        m_scanService->start(m_allBooks);
    }
}

void LibraryModel::removeBook(int row)
{
    if (row < 0 || row >= m_visibleBooks.size()) {
        return;
    }
    m_repository->removeBook(m_visibleBooks.at(row).sourceUrl);
}

bool LibraryModel::relinkBook(const QUrl &oldSourceUrl, const QUrl &newSourceUrl)
{
    QString error;
    if (!m_repository->relinkBook(oldSourceUrl, newSourceUrl, &error)) {
        setErrorMessage(error);
        return false;
    }
    setErrorMessage({});
    return true;
}

QVariantMap LibraryModel::book(const QUrl &sourceUrl) const
{
    const auto match = std::find_if(m_allBooks.cbegin(),
                                    m_allBooks.cend(),
                                    [&sourceUrl](const LibraryBook &book) {
                                        return book.sourceUrl == sourceUrl;
                                    });
    return match == m_allBooks.cend() ? QVariantMap() : toVariantMap(*match);
}

QVariantMap LibraryModel::bookAt(int row) const
{
    return row >= 0 && row < m_visibleBooks.size()
               ? toVariantMap(m_visibleBooks.at(row)) : QVariantMap();
}

bool LibraryModel::updateBooksMetadata(const QVariantList &sourceUrls,
                                       const QVariantMap &changes)
{
    QVector<QUrl> urls;
    QSet<QString> seen;
    for (const QVariant &value : sourceUrls) {
        const QUrl sourceUrl = value.toUrl();
        const QString key = selectionKey(sourceUrl);
        if (!sourceUrl.isEmpty() && !seen.contains(key)) {
            urls.append(sourceUrl);
            seen.insert(key);
        }
    }
    if (urls.isEmpty()) {
        setErrorMessage(tr("Select at least one book."));
        return false;
    }

    BookMetadataPatch patch;
    if (changes.contains(QStringLiteral("title"))) {
        const QString title = changes.value(QStringLiteral("title")).toString().trimmed().left(300);
        if (title.isEmpty()) {
            setErrorMessage(tr("A book title cannot be empty."));
            return false;
        }
        patch.title = title;
    }
    if (changes.contains(QStringLiteral("author"))) {
        patch.author = changes.value(QStringLiteral("author")).toString().trimmed().left(240);
    }
    if (changes.contains(QStringLiteral("series"))) {
        patch.series = changes.value(QStringLiteral("series")).toString().trimmed().left(240);
    }
    if (changes.contains(QStringLiteral("seriesNumber"))) {
        bool valid = false;
        const qreal seriesNumber = changes.value(QStringLiteral("seriesNumber")).toDouble(&valid);
        if (!valid || seriesNumber < 0 || seriesNumber > 100000) {
            setErrorMessage(tr("Enter a valid series number."));
            return false;
        }
        patch.seriesNumber = seriesNumber;
    }
    if (changes.contains(QStringLiteral("genres"))) {
        patch.genres = normalizedList(changes.value(QStringLiteral("genres")));
    }
    if (changes.contains(QStringLiteral("tags"))) {
        patch.tags = normalizedList(changes.value(QStringLiteral("tags")));
    }
    if (changes.contains(QStringLiteral("language"))) {
        patch.language = changes.value(QStringLiteral("language")).toString().trimmed().left(64);
    }
    if (changes.contains(QStringLiteral("publicationYear"))) {
        bool valid = false;
        const int year = changes.value(QStringLiteral("publicationYear")).toInt(&valid);
        if (!valid || year < 0 || year > 9999) {
            setErrorMessage(tr("Enter a valid publication year."));
            return false;
        }
        patch.publicationYear = year;
    }
    if (patch.isEmpty()) {
        setErrorMessage(tr("Choose at least one metadata field to update."));
        return false;
    }

    QString error;
    if (!m_repository->updateBookDetails(urls, patch, &error)) {
        setErrorMessage(error);
        return false;
    }
    setErrorMessage({});
    if (m_selectionMode) {
        setSelectionMode(false);
    }
    refresh();
    return true;
}

bool LibraryModel::setCustomCover(const QUrl &sourceUrl, const QUrl &imageUrl)
{
    QString error;
    if (!m_repository->setCustomCover(sourceUrl, imageUrl, &error)) {
        setErrorMessage(error);
        return false;
    }
    setErrorMessage({});
    refresh();
    return true;
}

void LibraryModel::toggleSelection(const QUrl &sourceUrl)
{
    const QString key = selectionKey(sourceUrl);
    if (key.isEmpty()) {
        return;
    }
    const bool exists = std::any_of(m_allBooks.cbegin(),
                                    m_allBooks.cend(),
                                    [&sourceUrl](const LibraryBook &book) {
                                        return book.sourceUrl == sourceUrl;
                                    });
    if (!exists) {
        return;
    }
    if (!m_selectionMode) {
        m_selectionMode = true;
        emit selectionModeChanged();
    }
    if (m_selectedBookKeys.contains(key)) {
        m_selectedBookKeys.remove(key);
    } else {
        m_selectedBookKeys.insert(key);
    }
    for (int row = 0; row < m_visibleBooks.size(); ++row) {
        if (m_visibleBooks.at(row).sourceUrl == sourceUrl) {
            emit dataChanged(index(row, 0), index(row, 0), {SelectedRole});
            break;
        }
    }
    emit selectionChanged();
}

void LibraryModel::selectAllVisible()
{
    if (!m_selectionMode) {
        m_selectionMode = true;
        emit selectionModeChanged();
    }
    bool changed = false;
    for (const LibraryBook &book : std::as_const(m_visibleBooks)) {
        const QString key = selectionKey(book.sourceUrl);
        if (!m_selectedBookKeys.contains(key)) {
            m_selectedBookKeys.insert(key);
            changed = true;
        }
    }
    if (!changed) {
        return;
    }
    if (!m_visibleBooks.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_visibleBooks.size() - 1, 0), {SelectedRole});
    }
    emit selectionChanged();
}

void LibraryModel::clearSelection()
{
    if (m_selectedBookKeys.isEmpty()) {
        return;
    }
    m_selectedBookKeys.clear();
    if (!m_visibleBooks.isEmpty()) {
        emit dataChanged(index(0, 0), index(m_visibleBooks.size() - 1, 0), {SelectedRole});
    }
    emit selectionChanged();
}

void LibraryModel::clearFilters()
{
    const bool changed = !m_filterText.isEmpty()
                         || m_formatFilter != QLatin1String("all")
                         || m_progressFilter != QLatin1String("all")
                         || m_genreFilter != QLatin1String("all")
                         || m_tagFilter != QLatin1String("all")
                         || m_languageFilter != QLatin1String("all")
                         || !m_collectionFilter.isEmpty();
    if (!changed) {
        return;
    }

    if (!m_filterText.isEmpty()) {
        m_filterText.clear();
        emit filterTextChanged();
    }
    if (m_formatFilter != QLatin1String("all")) {
        m_formatFilter = QStringLiteral("all");
        emit formatFilterChanged();
    }
    if (m_progressFilter != QLatin1String("all")) {
        m_progressFilter = QStringLiteral("all");
        emit progressFilterChanged();
    }
    if (!m_collectionFilter.isEmpty()) {
        m_collectionFilter.clear();
        emit collectionFilterChanged();
    }
    if (m_genreFilter != QLatin1String("all")) {
        m_genreFilter = QStringLiteral("all");
        emit genreFilterChanged();
    }
    if (m_tagFilter != QLatin1String("all")) {
        m_tagFilter = QStringLiteral("all");
        emit tagFilterChanged();
    }
    if (m_languageFilter != QLatin1String("all")) {
        m_languageFilter = QStringLiteral("all");
        emit languageFilterChanged();
    }
    rebuildVisibleBooks();
    emit hasActiveFiltersChanged();
}

void LibraryModel::clearError()
{
    setErrorMessage({});
}

void LibraryModel::rescan()
{
    if (m_scanService) {
        m_scanService->start(m_allBooks);
    }
}

void LibraryModel::cancelScan()
{
    if (m_scanService) {
        m_scanService->cancel();
    }
}

QVariantMap LibraryModel::toVariantMap(const LibraryBook &book)
{
    return {
        {QStringLiteral("sourceUrl"), book.sourceUrl},
        {QStringLiteral("sourcePath"), book.sourcePath},
        {QStringLiteral("title"), book.title},
        {QStringLiteral("author"), book.author},
        {QStringLiteral("series"), book.series},
        {QStringLiteral("seriesNumber"), book.seriesNumber},
        {QStringLiteral("genres"), book.genres},
        {QStringLiteral("tags"), book.tags},
        {QStringLiteral("language"), book.language},
        {QStringLiteral("publicationYear"), book.publicationYear},
        {QStringLiteral("formatName"), book.formatName},
        {QStringLiteral("coverUrl"), book.coverUrl},
        {QStringLiteral("customCoverUrl"), book.customCoverUrl},
        {QStringLiteral("progress"), book.progress},
        {QStringLiteral("lastOpened"), book.lastOpened},
        {QStringLiteral("fileAvailable"), book.fileAvailable},
        {QStringLiteral("collectionPath"), book.collectionPath},
        {QStringLiteral("cloudPlaceholder"), book.cloudPlaceholder}
    };
}

bool LibraryModel::matchesFilters(const LibraryBook &book) const
{
    const bool matchesText = m_filterText.isEmpty()
                             || book.title.contains(m_filterText, Qt::CaseInsensitive)
                             || book.author.contains(m_filterText, Qt::CaseInsensitive)
                             || book.series.contains(m_filterText, Qt::CaseInsensitive)
                             || book.genres.join(u' ').contains(m_filterText, Qt::CaseInsensitive)
                             || book.tags.join(u' ').contains(m_filterText, Qt::CaseInsensitive)
                             || book.language.contains(m_filterText, Qt::CaseInsensitive)
                             || book.formatName.contains(m_filterText, Qt::CaseInsensitive)
                             || book.collectionPath.contains(m_filterText, Qt::CaseInsensitive)
                             || book.sourcePath.contains(m_filterText, Qt::CaseInsensitive);
    const bool matchesFormat = m_formatFilter == QLatin1String("all")
                               || book.formatName.compare(m_formatFilter,
                                                          Qt::CaseInsensitive) == 0;
    const bool matchesCollection = m_collectionFilter.isEmpty()
                                   || (m_collectionFilter == QLatin1String(".")
                                       ? book.collectionPath.isEmpty()
                                       : book.collectionPath == m_collectionFilter
                                             || book.collectionPath.startsWith(
                                                 m_collectionFilter + u'/'));
    const bool matchesGenre = m_genreFilter == QLatin1String("all")
                              || containsValue(book.genres, m_genreFilter);
    const bool matchesTag = m_tagFilter == QLatin1String("all")
                            || containsValue(book.tags, m_tagFilter);
    const bool matchesLanguage = m_languageFilter == QLatin1String("all")
                                 || book.language.compare(m_languageFilter,
                                                          Qt::CaseInsensitive) == 0;

    bool matchesProgress = true;
    if (m_progressFilter == QLatin1String("unread")) {
        matchesProgress = book.progress <= unreadProgressThreshold;
    } else if (m_progressFilter == QLatin1String("reading")) {
        matchesProgress = book.progress > unreadProgressThreshold
                          && book.progress < finishedProgressThreshold;
    } else if (m_progressFilter == QLatin1String("finished")) {
        matchesProgress = book.progress >= finishedProgressThreshold;
    }
    return matchesText && matchesFormat && matchesCollection && matchesGenre
           && matchesTag && matchesLanguage && matchesProgress;
}

void LibraryModel::rebuildVisibleBooks()
{
    const int previousCount = m_visibleBooks.size();

    beginResetModel();
    m_visibleBooks.clear();
    m_visibleBooks.reserve(m_allBooks.size());
    for (const LibraryBook &book : std::as_const(m_allBooks)) {
        if (matchesFilters(book)) {
            m_visibleBooks.append(book);
        }
    }

    if (m_sortMode == QLatin1String("title")) {
        std::stable_sort(m_visibleBooks.begin(),
                         m_visibleBooks.end(),
                         [](const LibraryBook &left, const LibraryBook &right) {
                             return compareText(left.title, right.title) < 0;
                         });
    } else if (m_sortMode == QLatin1String("author")) {
        std::stable_sort(m_visibleBooks.begin(),
                         m_visibleBooks.end(),
                         [](const LibraryBook &left, const LibraryBook &right) {
                             const QString leftAuthor = left.author.isEmpty()
                                                            ? left.title
                                                            : left.author;
                             const QString rightAuthor = right.author.isEmpty()
                                                             ? right.title
                                                             : right.author;
                             const int authorOrder = compareText(leftAuthor, rightAuthor);
                             return authorOrder == 0
                                        ? compareText(left.title, right.title) < 0
                                        : authorOrder < 0;
                         });
    } else if (m_sortMode == QLatin1String("series")) {
        std::stable_sort(m_visibleBooks.begin(),
                         m_visibleBooks.end(),
                         [](const LibraryBook &left, const LibraryBook &right) {
                             const QString leftSeries = left.series.isEmpty()
                                                            ? left.title
                                                            : left.series;
                             const QString rightSeries = right.series.isEmpty()
                                                             ? right.title
                                                             : right.series;
                             const int seriesOrder = compareText(leftSeries, rightSeries);
                             if (seriesOrder != 0) {
                                 return seriesOrder < 0;
                             }
                             if (!qFuzzyCompare(left.seriesNumber + 1,
                                                right.seriesNumber + 1)) {
                                 return left.seriesNumber < right.seriesNumber;
                             }
                             return compareText(left.title, right.title) < 0;
                         });
    } else if (m_sortMode == QLatin1String("year")) {
        std::stable_sort(m_visibleBooks.begin(),
                         m_visibleBooks.end(),
                         [](const LibraryBook &left, const LibraryBook &right) {
                             if (left.publicationYear != right.publicationYear) {
                                 if (left.publicationYear == 0) {
                                     return false;
                                 }
                                 if (right.publicationYear == 0) {
                                     return true;
                                 }
                                 return left.publicationYear > right.publicationYear;
                             }
                             return compareText(left.title, right.title) < 0;
                         });
    }
    endResetModel();

    if (previousCount != m_visibleBooks.size()) {
        emit countChanged();
    }
}

void LibraryModel::rebuildAvailableFormats()
{
    QStringList formats;
    for (const LibraryBook &book : std::as_const(m_allBooks)) {
        if (!book.formatName.isEmpty()
            && !formats.contains(book.formatName, Qt::CaseInsensitive)) {
            formats.append(book.formatName);
        }
    }
    std::sort(formats.begin(), formats.end(), [](const QString &left, const QString &right) {
        return compareText(left, right) < 0;
    });
    if (m_availableFormats != formats) {
        m_availableFormats = formats;
        emit availableFormatsChanged();
    }
}

void LibraryModel::rebuildMetadataFilters()
{
    QStringList genres;
    QStringList tags;
    QStringList languages;
    const auto appendUnique = [](QStringList *target, const QString &value) {
        const QString normalized = value.trimmed();
        if (!normalized.isEmpty() && !target->contains(normalized, Qt::CaseInsensitive)) {
            target->append(normalized);
        }
    };

    for (const LibraryBook &book : std::as_const(m_allBooks)) {
        for (const QString &genre : book.genres) {
            appendUnique(&genres, genre);
        }
        for (const QString &tag : book.tags) {
            appendUnique(&tags, tag);
        }
        appendUnique(&languages, book.language);
    }
    const auto sortValues = [](QStringList *values) {
        std::sort(values->begin(), values->end(), [](const QString &left, const QString &right) {
            return compareText(left, right) < 0;
        });
    };
    sortValues(&genres);
    sortValues(&tags);
    sortValues(&languages);

    if (m_availableGenres == genres
        && m_availableTags == tags
        && m_availableLanguages == languages) {
        return;
    }
    m_availableGenres = genres;
    m_availableTags = tags;
    m_availableLanguages = languages;
    emit metadataFiltersChanged();
}

void LibraryModel::pruneSelection()
{
    QSet<QString> availableKeys;
    for (const LibraryBook &book : std::as_const(m_allBooks)) {
        availableKeys.insert(selectionKey(book.sourceUrl));
    }
    QSet<QString> retained = m_selectedBookKeys;
    retained.intersect(availableKeys);
    if (retained == m_selectedBookKeys) {
        return;
    }
    m_selectedBookKeys = retained;
    emit selectionChanged();
}

void LibraryModel::rebuildBookRows()
{
    m_bookRows.clear();
    m_bookRows.reserve(m_allBooks.size());
    for (int row = 0; row < m_allBooks.size(); ++row) {
        m_bookRows.insert(selectionKey(m_allBooks.at(row).sourceUrl), row);
    }
}

void LibraryModel::applyScanResults(const QVector<LibraryScanResult> &results)
{
    bool changed = false;
    bool metadataChanged = false;
    bool recentChanged = false;
    for (const LibraryScanResult &result : results) {
        const auto row = m_bookRows.constFind(selectionKey(result.sourceUrl));
        if (row == m_bookRows.cend()) {
            continue;
        }

        LibraryBook &book = m_allBooks[*row];
        if (book.fileAvailable != result.fileAvailable
            || book.cloudPlaceholder != result.cloudPlaceholder) {
            book.fileAvailable = result.fileAvailable;
            book.cloudPlaceholder = result.cloudPlaceholder;
            changed = true;
        }
        if (!result.metadataInspected) {
            recentChanged = recentChanged || (*row == 0 && changed);
            continue;
        }

        const BookMetadata &metadata = result.metadata;
        if (!book.metadataEdited) {
            if (metadata.hasDocumentTitle || book.title.isEmpty()) {
                book.title = metadata.title;
            }
            if (metadata.hasDocumentAuthor || book.author.isEmpty()) {
                book.author = metadata.author;
            }
        }
        if (!metadata.formatName.isEmpty()) {
            book.formatName = metadata.formatName;
        }
        book.metadataFingerprint = metadata.fingerprint;
        const bool customCoverAvailable = !book.customCoverUrl.isEmpty()
                                          && (!book.customCoverUrl.isLocalFile()
                                              || QFileInfo::exists(
                                                  book.customCoverUrl.toLocalFile()));
        book.coverUrl = customCoverAvailable ? book.customCoverUrl : metadata.coverUrl;
        m_repository->persistScannedMetadata(result.sourceUrl, metadata);
        changed = true;
        metadataChanged = true;
        recentChanged = recentChanged || *row == 0;
    }

    if (!changed) {
        return;
    }
    if (metadataChanged) {
        rebuildAvailableFormats();
        rebuildMetadataFilters();
    }
    rebuildVisibleBooks();
    if (recentChanged) {
        emit recentBookChanged();
    }
}

void LibraryModel::updateProgress(const QUrl &sourceUrl, qreal progress)
{
    progress = qBound(qreal(0), progress, qreal(1));
    bool recentBookUpdated = false;
    for (qsizetype row = 0; row < m_allBooks.size(); ++row) {
        LibraryBook &book = m_allBooks[row];
        if (book.sourceUrl == sourceUrl) {
            book.progress = progress;
            recentBookUpdated = row == 0;
            break;
        }
    }
    rebuildVisibleBooks();
    if (recentBookUpdated) {
        emit recentBookChanged();
    }
}

void LibraryModel::setErrorMessage(const QString &errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }
    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}
