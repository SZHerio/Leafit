#pragma once

#include "../reader/readingannotation.h"

#include <QObject>
#include <QSettings>
#include <QUrl>
#include <QVariantList>
#include <QVector>

class ReadingAnnotationStore final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QUrl documentUrl READ documentUrl WRITE setDocumentUrl NOTIFY documentUrlChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY annotationsChanged)
    Q_PROPERTY(QVariantList highlights READ highlights NOTIFY annotationsChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY annotationsChanged)

public:
    explicit ReadingAnnotationStore(const QString &settingsFilePath, QObject *parent = nullptr);

    QUrl documentUrl() const;
    QVariantList bookmarks() const;
    QVariantList highlights() const;
    int totalCount() const;

    void setDocumentUrl(const QUrl &documentUrl);

    Q_INVOKABLE bool hasBookmark(qreal progress, int page) const;
    Q_INVOKABLE bool toggleBookmark(qreal progress, int page, const QString &label);
    Q_INVOKABLE QString addHighlight(int start,
                                     int length,
                                     const QString &excerpt,
                                     const QString &note,
                                     qreal progress,
                                     int page);
    Q_INVOKABLE void updateNote(const QString &annotationId, const QString &note);
    Q_INVOKABLE void removeAnnotation(const QString &annotationId);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void sync();

signals:
    void documentUrlChanged();
    void annotationsChanged();
    void profileChanged();

private:
    static QVariantMap toVariantMap(const ReadingAnnotation &annotation);
    static QString normalizedExcerpt(const QString &text);
    static QString normalizedNote(const QString &text);
    static QString typeName(ReadingAnnotationType type);
    static ReadingAnnotationType typeFromName(const QString &name);

    int bookmarkIndex(qreal progress, int page) const;
    QString documentGroup() const;
    void loadAnnotations();
    void saveAnnotation(const ReadingAnnotation &annotation);
    void removeStoredAnnotation(const QString &annotationId);
    void sortAnnotations();

    mutable QSettings m_settings;
    QUrl m_documentUrl;
    QVector<ReadingAnnotation> m_annotations;
};
