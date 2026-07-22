#pragma once

#include "librarybook.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

class LibraryRepository;
class LibraryScanService;
struct LibraryScanResult;

class LibraryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY sortModeChanged)
    Q_PROPERTY(QString formatFilter READ formatFilter WRITE setFormatFilter NOTIFY formatFilterChanged)
    Q_PROPERTY(QString progressFilter READ progressFilter WRITE setProgressFilter NOTIFY progressFilterChanged)
    Q_PROPERTY(QString collectionFilter READ collectionFilter WRITE setCollectionFilter NOTIFY collectionFilterChanged)
    Q_PROPERTY(QString genreFilter READ genreFilter WRITE setGenreFilter NOTIFY genreFilterChanged)
    Q_PROPERTY(QString tagFilter READ tagFilter WRITE setTagFilter NOTIFY tagFilterChanged)
    Q_PROPERTY(QString languageFilter READ languageFilter WRITE setLanguageFilter NOTIFY languageFilterChanged)
    Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(QStringList availableFormats READ availableFormats NOTIFY availableFormatsChanged)
    Q_PROPERTY(QStringList availableGenres READ availableGenres NOTIFY metadataFiltersChanged)
    Q_PROPERTY(QStringList availableTags READ availableTags NOTIFY metadataFiltersChanged)
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages NOTIFY metadataFiltersChanged)
    Q_PROPERTY(bool hasActiveFilters READ hasActiveFilters NOTIFY hasActiveFiltersChanged)
    Q_PROPERTY(bool selectionMode READ selectionMode WRITE setSelectionMode NOTIFY selectionModeChanged)
    Q_PROPERTY(int selectionCount READ selectionCount NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList selectedBooks READ selectedBooks NOTIFY selectionChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(QVariantMap recentBook READ recentBook NOTIFY recentBookChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanStateChanged)
    Q_PROPERTY(bool scanCancellationRequested READ scanCancellationRequested NOTIFY scanStateChanged)
    Q_PROPERTY(int scanCompleted READ scanCompleted NOTIFY scanProgressChanged)
    Q_PROPERTY(int scanTotal READ scanTotal NOTIFY scanProgressChanged)
    Q_PROPERTY(qreal scanProgress READ scanProgress NOTIFY scanProgressChanged)

public:
    enum Role {
        SourceUrlRole = Qt::UserRole + 1,
        SourcePathRole,
        TitleRole,
        AuthorRole,
        SeriesRole,
        SeriesNumberRole,
        GenresRole,
        TagsRole,
        LanguageRole,
        PublicationYearRole,
        FormatNameRole,
        CoverUrlRole,
        ProgressRole,
        LastOpenedRole,
        FileAvailableRole,
        CollectionPathRole,
        CloudPlaceholderRole,
        SelectedRole
    };
    Q_ENUM(Role)

    explicit LibraryModel(LibraryRepository *repository, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    QString sortMode() const;
    QString formatFilter() const;
    QString progressFilter() const;
    QString collectionFilter() const;
    QString genreFilter() const;
    QString tagFilter() const;
    QString languageFilter() const;
    QString viewMode() const;
    QStringList availableFormats() const;
    QStringList availableGenres() const;
    QStringList availableTags() const;
    QStringList availableLanguages() const;
    bool hasActiveFilters() const;
    bool selectionMode() const;
    int selectionCount() const;
    QVariantList selectedBooks() const;
    QString errorMessage() const;
    int totalCount() const;
    QVariantMap recentBook() const;
    bool scanning() const;
    bool scanCancellationRequested() const;
    int scanCompleted() const;
    int scanTotal() const;
    qreal scanProgress() const;

    void setScanService(LibraryScanService *scanService);

    void setFilterText(const QString &filterText);
    void setSortMode(const QString &sortMode);
    void setFormatFilter(const QString &formatFilter);
    void setProgressFilter(const QString &progressFilter);
    void setCollectionFilter(const QString &collectionFilter);
    void setGenreFilter(const QString &genreFilter);
    void setTagFilter(const QString &tagFilter);
    void setLanguageFilter(const QString &languageFilter);
    void setViewMode(const QString &viewMode);
    void setSelectionMode(bool selectionMode);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void removeBook(int row);
    Q_INVOKABLE bool relinkBook(const QUrl &oldSourceUrl, const QUrl &newSourceUrl);
    Q_INVOKABLE QVariantMap book(const QUrl &sourceUrl) const;
    Q_INVOKABLE QVariantMap bookAt(int row) const;
    Q_INVOKABLE bool updateBooksMetadata(const QVariantList &sourceUrls,
                                         const QVariantMap &changes);
    Q_INVOKABLE bool setCustomCover(const QUrl &sourceUrl, const QUrl &imageUrl);
    Q_INVOKABLE void toggleSelection(const QUrl &sourceUrl);
    Q_INVOKABLE void selectAllVisible();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void clearFilters();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void rescan();
    Q_INVOKABLE void cancelScan();

signals:
    void filterTextChanged();
    void sortModeChanged();
    void formatFilterChanged();
    void progressFilterChanged();
    void collectionFilterChanged();
    void genreFilterChanged();
    void tagFilterChanged();
    void languageFilterChanged();
    void viewModeChanged();
    void availableFormatsChanged();
    void metadataFiltersChanged();
    void hasActiveFiltersChanged();
    void selectionModeChanged();
    void selectionChanged();
    void errorMessageChanged();
    void countChanged();
    void totalCountChanged();
    void recentBookChanged();
    void scanStateChanged();
    void scanProgressChanged();

private:
    static QVariantMap toVariantMap(const LibraryBook &book);
    bool matchesFilters(const LibraryBook &book) const;
    void rebuildVisibleBooks();
    void rebuildAvailableFormats();
    void rebuildMetadataFilters();
    void pruneSelection();
    void rebuildBookRows();
    void applyScanResults(const QVector<LibraryScanResult> &results);
    void updateProgress(const QUrl &sourceUrl, qreal progress);
    void setErrorMessage(const QString &errorMessage);

    LibraryRepository *m_repository = nullptr;
    LibraryScanService *m_scanService = nullptr;
    QVector<LibraryBook> m_allBooks;
    QVector<LibraryBook> m_visibleBooks;
    QHash<QString, int> m_bookRows;
    QStringList m_availableFormats;
    QString m_filterText;
    QString m_sortMode = QStringLiteral("recent");
    QString m_formatFilter = QStringLiteral("all");
    QString m_progressFilter = QStringLiteral("all");
    QString m_collectionFilter;
    QString m_genreFilter = QStringLiteral("all");
    QString m_tagFilter = QStringLiteral("all");
    QString m_languageFilter = QStringLiteral("all");
    QString m_viewMode = QStringLiteral("grid");
    QString m_errorMessage;
    QStringList m_availableGenres;
    QStringList m_availableTags;
    QStringList m_availableLanguages;
    QSet<QString> m_selectedBookKeys;
    bool m_selectionMode = false;
};
