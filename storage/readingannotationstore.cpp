#include "readingannotationstore.h"

#include "profiledatabase.h"

#include <QRegularExpression>
#include <QSettings>
#include <QUuid>

#include <algorithm>

namespace {

constexpr qreal bookmarkProgressTolerance = 0.003;
constexpr int maximumExcerptLength = 180;
constexpr int maximumNoteLength = 2000;

qreal boundedProgress(qreal progress)
{
    return qBound(qreal(0), progress, qreal(1));
}

} // namespace

ReadingAnnotationStore::ReadingAnnotationStore(const QString &settingsFilePath, QObject *parent)
    : QObject(parent)
    , m_ownedDatabase(std::make_unique<ProfileDatabase>(
          ProfileDatabase::databasePathForSettings(settingsFilePath)))
    , m_database(m_ownedDatabase.get())
{
    QSettings settings(settingsFilePath, QSettings::IniFormat);
    m_database->migrateLegacySettings(&settings);
}

ReadingAnnotationStore::ReadingAnnotationStore(ProfileDatabase *database, QObject *parent)
    : QObject(parent)
    , m_database(database)
{
    Q_ASSERT(m_database);
}

ReadingAnnotationStore::~ReadingAnnotationStore() = default;

QUrl ReadingAnnotationStore::documentUrl() const
{
    return m_documentUrl;
}

QVariantList ReadingAnnotationStore::bookmarks() const
{
    QVariantList values;
    for (const ReadingAnnotation &annotation : m_annotations) {
        if (annotation.type == ReadingAnnotationType::Bookmark) {
            values.append(toVariantMap(annotation));
        }
    }
    return values;
}

QVariantList ReadingAnnotationStore::highlights() const
{
    QVariantList values;
    for (const ReadingAnnotation &annotation : m_annotations) {
        if (annotation.type == ReadingAnnotationType::Highlight) {
            values.append(toVariantMap(annotation));
        }
    }
    return values;
}

int ReadingAnnotationStore::totalCount() const
{
    return m_annotations.size();
}

void ReadingAnnotationStore::setDocumentUrl(const QUrl &documentUrl)
{
    if (m_documentUrl == documentUrl) {
        return;
    }

    m_documentUrl = documentUrl;
    loadAnnotations();
    emit documentUrlChanged();
    emit annotationsChanged();
}

bool ReadingAnnotationStore::hasBookmark(qreal progress, int page) const
{
    return bookmarkIndex(progress, page) >= 0;
}

bool ReadingAnnotationStore::toggleBookmark(qreal progress, int page, const QString &label)
{
    if (m_documentUrl.isEmpty()) {
        return false;
    }

    const int existingIndex = bookmarkIndex(progress, page);
    if (existingIndex >= 0) {
        const QString annotationId = m_annotations.at(existingIndex).id;
        if (!removeStoredAnnotation(annotationId)) {
            return true;
        }
        m_annotations.removeAt(existingIndex);
        emit annotationsChanged();
        emit profileChanged();
        return false;
    }

    ReadingAnnotation annotation;
    annotation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    annotation.type = ReadingAnnotationType::Bookmark;
    annotation.progress = boundedProgress(progress);
    annotation.page = page;
    annotation.label = label.trimmed();
    annotation.createdAt = QDateTime::currentDateTimeUtc();
    if (!saveAnnotation(annotation)) {
        return false;
    }
    m_annotations.append(annotation);
    sortAnnotations();
    emit annotationsChanged();
    emit profileChanged();
    return true;
}

QString ReadingAnnotationStore::addHighlight(int start,
                                             int length,
                                             const QString &excerpt,
                                             const QString &note,
                                             qreal progress,
                                             int page)
{
    const QString cleanExcerpt = normalizedExcerpt(excerpt);
    if (m_documentUrl.isEmpty() || cleanExcerpt.isEmpty()) {
        return {};
    }
    if (page < 0 && (start < 0 || length <= 0)) {
        return {};
    }

    for (ReadingAnnotation &annotation : m_annotations) {
        if (annotation.type != ReadingAnnotationType::Highlight) {
            continue;
        }
        const bool sameTextRange = page < 0
                                   && annotation.page < 0
                                   && annotation.start == start
                                   && annotation.length == length;
        const bool samePdfSelection = page >= 0
                                      && annotation.page == page
                                      && annotation.excerpt == cleanExcerpt;
        if (sameTextRange || samePdfSelection) {
            ReadingAnnotation updated = annotation;
            updated.note = normalizedNote(note);
            if (!saveAnnotation(updated)) {
                return {};
            }
            annotation = updated;
            emit annotationsChanged();
            emit profileChanged();
            return annotation.id;
        }
    }

    ReadingAnnotation annotation;
    annotation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    annotation.type = ReadingAnnotationType::Highlight;
    annotation.progress = boundedProgress(progress);
    annotation.page = page;
    annotation.start = start;
    annotation.length = length;
    annotation.excerpt = cleanExcerpt;
    annotation.note = normalizedNote(note);
    annotation.createdAt = QDateTime::currentDateTimeUtc();
    if (!saveAnnotation(annotation)) {
        return {};
    }
    m_annotations.append(annotation);
    sortAnnotations();
    emit annotationsChanged();
    emit profileChanged();
    return annotation.id;
}

void ReadingAnnotationStore::updateNote(const QString &annotationId, const QString &note)
{
    for (ReadingAnnotation &annotation : m_annotations) {
        if (annotation.id != annotationId) {
            continue;
        }

        const QString cleanNote = normalizedNote(note);
        if (annotation.note == cleanNote) {
            return;
        }
        ReadingAnnotation updated = annotation;
        updated.note = cleanNote;
        if (!saveAnnotation(updated)) {
            return;
        }
        annotation = updated;
        emit annotationsChanged();
        emit profileChanged();
        return;
    }
}

void ReadingAnnotationStore::removeAnnotation(const QString &annotationId)
{
    const auto iterator = std::find_if(m_annotations.begin(),
                                       m_annotations.end(),
                                       [&annotationId](const ReadingAnnotation &annotation) {
                                           return annotation.id == annotationId;
                                       });
    if (iterator == m_annotations.end()) {
        return;
    }

    if (!removeStoredAnnotation(annotationId)) {
        return;
    }
    m_annotations.erase(iterator);
    emit annotationsChanged();
    emit profileChanged();
}

void ReadingAnnotationStore::reload()
{
    loadAnnotations();
    emit annotationsChanged();
}

void ReadingAnnotationStore::sync()
{
    m_database->sync();
}

QVariantMap ReadingAnnotationStore::toVariantMap(const ReadingAnnotation &annotation)
{
    return {
        {QStringLiteral("id"), annotation.id},
        {QStringLiteral("type"), typeName(annotation.type)},
        {QStringLiteral("progress"), annotation.progress},
        {QStringLiteral("page"), annotation.page},
        {QStringLiteral("start"), annotation.start},
        {QStringLiteral("length"), annotation.length},
        {QStringLiteral("label"), annotation.label},
        {QStringLiteral("excerpt"), annotation.excerpt},
        {QStringLiteral("note"), annotation.note},
        {QStringLiteral("createdAt"), annotation.createdAt.toString(Qt::ISODateWithMs)}
    };
}

QString ReadingAnnotationStore::normalizedExcerpt(const QString &text)
{
    QString normalized = text;
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    normalized = normalized.trimmed();
    if (normalized.size() > maximumExcerptLength) {
        normalized = normalized.left(maximumExcerptLength - 3) + QStringLiteral("...");
    }
    return normalized;
}

QString ReadingAnnotationStore::normalizedNote(const QString &text)
{
    return text.trimmed().left(maximumNoteLength);
}

QString ReadingAnnotationStore::typeName(ReadingAnnotationType type)
{
    return type == ReadingAnnotationType::Bookmark
               ? QStringLiteral("bookmark")
               : QStringLiteral("highlight");
}

int ReadingAnnotationStore::bookmarkIndex(qreal progress, int page) const
{
    progress = boundedProgress(progress);
    for (int index = 0; index < m_annotations.size(); ++index) {
        const ReadingAnnotation &annotation = m_annotations.at(index);
        if (annotation.type != ReadingAnnotationType::Bookmark) {
            continue;
        }
        if (page >= 0 && annotation.page == page) {
            return index;
        }
        if (page < 0
            && annotation.page < 0
            && qAbs(annotation.progress - progress) <= bookmarkProgressTolerance) {
            return index;
        }
    }
    return -1;
}

void ReadingAnnotationStore::loadAnnotations()
{
    m_annotations.clear();
    if (m_documentUrl.isEmpty()) {
        return;
    }
    m_annotations = m_database->annotations(m_documentUrl);
    sortAnnotations();
}

bool ReadingAnnotationStore::saveAnnotation(const ReadingAnnotation &annotation)
{
    if (m_documentUrl.isEmpty()) {
        return false;
    }
    return m_database->saveAnnotation(m_documentUrl, annotation);
}

bool ReadingAnnotationStore::removeStoredAnnotation(const QString &annotationId)
{
    if (m_documentUrl.isEmpty()) {
        return false;
    }
    return m_database->removeAnnotation(m_documentUrl, annotationId);
}

void ReadingAnnotationStore::sortAnnotations()
{
    std::sort(m_annotations.begin(),
              m_annotations.end(),
              [](const ReadingAnnotation &left, const ReadingAnnotation &right) {
                  if (left.page >= 0 || right.page >= 0) {
                      if (left.page != right.page) {
                          return left.page < right.page;
                      }
                  } else if (!qFuzzyCompare(left.progress, right.progress)) {
                      return left.progress < right.progress;
                  }
                  return left.createdAt < right.createdAt;
              });
}
