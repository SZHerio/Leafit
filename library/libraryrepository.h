#pragma once

#include "librarybook.h"

#include <QObject>
#include <QVector>

class BookMetadataService;
class LocalStateStore;

class LibraryRepository final : public QObject
{
    Q_OBJECT

public:
    explicit LibraryRepository(LocalStateStore *store,
                               BookMetadataService *metadataService,
                               QObject *parent = nullptr);

    QVector<LibraryBook> books();
    QString sortMode() const;
    QString viewMode() const;

    void setSortMode(const QString &sortMode);
    void setViewMode(const QString &viewMode);
    void recordBookOpened(const QUrl &sourceUrl,
                          const QString &title,
                          const QString &author,
                          const QString &formatName);
    bool registerBook(const QUrl &sourceUrl,
                      const QString &collectionPath,
                      bool restoreExcluded = false);
    bool supports(const QUrl &sourceUrl) const;
    void removeBook(const QUrl &sourceUrl);
    bool relinkBook(const QUrl &oldSourceUrl,
                    const QUrl &newSourceUrl,
                    QString *errorMessage = nullptr);

signals:
    void changed();
    void documentProgressChanged(const QUrl &sourceUrl, qreal progress);

private:
    LocalStateStore *m_store = nullptr;
    BookMetadataService *m_metadataService = nullptr;
};
