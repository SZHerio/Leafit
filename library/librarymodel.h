#pragma once

#include "librarybook.h"

#include <QAbstractListModel>
#include <QVariantMap>

class LocalStateStore;

class LibraryModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(QVariantMap recentBook READ recentBook NOTIFY recentBookChanged)

public:
    enum Role {
        SourceUrlRole = Qt::UserRole + 1,
        SourcePathRole,
        TitleRole,
        AuthorRole,
        FormatNameRole,
        ProgressRole,
        LastOpenedRole,
        FileAvailableRole
    };
    Q_ENUM(Role)

    explicit LibraryModel(LocalStateStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    int totalCount() const;
    QVariantMap recentBook() const;

    void setFilterText(const QString &filterText);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void removeBook(int row);

signals:
    void filterTextChanged();
    void countChanged();
    void totalCountChanged();
    void recentBookChanged();

private:
    static QVariantMap toVariantMap(const LibraryBook &book);
    static bool matchesFilter(const LibraryBook &book, const QString &filterText);
    void rebuildVisibleBooks();
    void updateProgress(const QUrl &sourceUrl, qreal progress);

    LocalStateStore *m_store = nullptr;
    QVector<LibraryBook> m_allBooks;
    QVector<LibraryBook> m_visibleBooks;
    QString m_filterText;
};
