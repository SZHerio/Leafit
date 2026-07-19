#include "readingannotationstore.h"

#include "documentstoragekey.h"

#include <QRegularExpression>
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
    , m_settings(settingsFilePath, QSettings::IniFormat)
{
}

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
        m_annotations.removeAt(existingIndex);
        removeStoredAnnotation(annotationId);
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
    m_annotations.append(annotation);
    sortAnnotations();
    saveAnnotation(annotation);
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
            annotation.note = normalizedNote(note);
            saveAnnotation(annotation);
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
    m_annotations.append(annotation);
    sortAnnotations();
    saveAnnotation(annotation);
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
        annotation.note = cleanNote;
        saveAnnotation(annotation);
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

    m_annotations.erase(iterator);
    removeStoredAnnotation(annotationId);
    emit annotationsChanged();
    emit profileChanged();
}

void ReadingAnnotationStore::reload()
{
    m_settings.sync();
    loadAnnotations();
    emit annotationsChanged();
}

void ReadingAnnotationStore::sync()
{
    m_settings.sync();
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

ReadingAnnotationType ReadingAnnotationStore::typeFromName(const QString &name)
{
    return name == QLatin1String("highlight")
               ? ReadingAnnotationType::Highlight
               : ReadingAnnotationType::Bookmark;
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

QString ReadingAnnotationStore::documentGroup() const
{
    return QStringLiteral("annotations/%1").arg(DocumentStorageKey::id(m_documentUrl));
}

void ReadingAnnotationStore::loadAnnotations()
{
    m_annotations.clear();
    if (m_documentUrl.isEmpty()) {
        return;
    }

    m_settings.sync();
    m_settings.beginGroup(documentGroup());
    const QStringList annotationIds = m_settings.childGroups();
    m_annotations.reserve(annotationIds.size());
    for (const QString &annotationId : annotationIds) {
        m_settings.beginGroup(annotationId);
        ReadingAnnotation annotation;
        annotation.id = annotationId;
        annotation.type = typeFromName(m_settings.value(QStringLiteral("type")).toString());
        annotation.progress = boundedProgress(
            m_settings.value(QStringLiteral("progress"), 0).toReal());
        annotation.page = m_settings.value(QStringLiteral("page"), -1).toInt();
        annotation.start = m_settings.value(QStringLiteral("start"), -1).toInt();
        annotation.length = qMax(0, m_settings.value(QStringLiteral("length"), 0).toInt());
        annotation.label = m_settings.value(QStringLiteral("label")).toString();
        annotation.excerpt = m_settings.value(QStringLiteral("excerpt")).toString();
        annotation.note = m_settings.value(QStringLiteral("note")).toString();
        annotation.createdAt = QDateTime::fromString(
            m_settings.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
        if (!annotation.createdAt.isValid()) {
            annotation.createdAt = QDateTime::fromString(
                m_settings.value(QStringLiteral("createdAt")).toString(), Qt::ISODate);
        }
        m_annotations.append(annotation);
        m_settings.endGroup();
    }
    m_settings.endGroup();
    sortAnnotations();
}

void ReadingAnnotationStore::saveAnnotation(const ReadingAnnotation &annotation)
{
    if (m_documentUrl.isEmpty()) {
        return;
    }

    m_settings.beginGroup(documentGroup());
    m_settings.setValue(QStringLiteral("sourceUrl"),
                        m_documentUrl.toString(QUrl::FullyEncoded));
    m_settings.beginGroup(annotation.id);
    m_settings.setValue(QStringLiteral("type"), typeName(annotation.type));
    m_settings.setValue(QStringLiteral("progress"), annotation.progress);
    m_settings.setValue(QStringLiteral("page"), annotation.page);
    m_settings.setValue(QStringLiteral("start"), annotation.start);
    m_settings.setValue(QStringLiteral("length"), annotation.length);
    m_settings.setValue(QStringLiteral("label"), annotation.label);
    m_settings.setValue(QStringLiteral("excerpt"), annotation.excerpt);
    m_settings.setValue(QStringLiteral("note"), annotation.note);
    m_settings.setValue(QStringLiteral("createdAt"),
                        annotation.createdAt.toString(Qt::ISODateWithMs));
    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.sync();
}

void ReadingAnnotationStore::removeStoredAnnotation(const QString &annotationId)
{
    if (m_documentUrl.isEmpty()) {
        return;
    }

    m_settings.beginGroup(documentGroup());
    m_settings.remove(annotationId);
    m_settings.endGroup();
    m_settings.sync();
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
