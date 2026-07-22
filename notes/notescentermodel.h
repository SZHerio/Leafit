#pragma once

#include "../reader/readingannotation.h"

#include <QAbstractListModel>
#include <QUrl>
#include <QVector>

class ProfileDatabase;

class NotesCenterModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countsChanged)
    Q_PROPERTY(int bookmarkCount READ bookmarkCount NOTIFY countsChanged)
    Q_PROPERTY(int highlightCount READ highlightCount NOTIFY countsChanged)
    Q_PROPERTY(int noteCount READ noteCount NOTIFY countsChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(QUrl lastExportUrl READ lastExportUrl NOTIFY lastExportUrlChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        TypeRole,
        SourceUrlRole,
        BookTitleRole,
        BookAuthorRole,
        FormatNameRole,
        ProgressRole,
        PageRole,
        StartRole,
        LengthRole,
        LabelRole,
        ExcerptRole,
        NoteRole,
        CreatedAtRole
    };
    Q_ENUM(Role)

    explicit NotesCenterModel(ProfileDatabase *database, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString query() const;
    QString typeFilter() const;
    int totalCount() const;
    int bookmarkCount() const;
    int highlightCount() const;
    int noteCount() const;
    QString errorMessage() const;
    QUrl lastExportUrl() const;

    void setQuery(const QString &query);
    void setTypeFilter(const QString &typeFilter);

    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE QVariantMap find(const QUrl &sourceUrl, const QString &annotationId) const;
    Q_INVOKABLE bool updateAnnotation(const QUrl &sourceUrl,
                                      const QString &annotationId,
                                      const QString &label,
                                      const QString &note);
    Q_INVOKABLE bool removeAnnotation(const QUrl &sourceUrl,
                                      const QString &annotationId);
    Q_INVOKABLE bool exportFiltered(const QUrl &fileUrl, const QString &format);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearError();

signals:
    void queryChanged();
    void typeFilterChanged();
    void countChanged();
    void countsChanged();
    void errorMessageChanged();
    void lastExportUrlChanged();
    void annotationChanged(const QUrl &sourceUrl);
    void profileChanged();
    void exportCompleted(const QUrl &fileUrl, int annotationCount);

private:
    static QVariantMap toVariantMap(const LibraryReadingAnnotation &entry);
    static QString normalizedLabel(const QString &label);
    static QString normalizedNote(const QString &note);
    static QString normalizedTypeFilter(const QString &typeFilter);
    bool matchesFilter(const LibraryReadingAnnotation &entry) const;
    QString locationText(const ReadingAnnotation &annotation) const;
    QByteArray markdownExport() const;
    QByteArray htmlExport() const;
    void applyFilter();
    void updateCounts();
    void setErrorMessage(const QString &errorMessage);

    ProfileDatabase *m_database = nullptr;
    QVector<LibraryReadingAnnotation> m_allEntries;
    QVector<LibraryReadingAnnotation> m_visibleEntries;
    QString m_query;
    QString m_typeFilter = QStringLiteral("all");
    QString m_errorMessage;
    QUrl m_lastExportUrl;
    int m_bookmarkCount = 0;
    int m_highlightCount = 0;
    int m_noteCount = 0;
};
