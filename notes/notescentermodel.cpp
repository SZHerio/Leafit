#include "notescentermodel.h"

#include "../storage/profiledatabase.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QSaveFile>
#include <QtMath>

#include <algorithm>

namespace {

constexpr int maximumLabelLength = 240;
constexpr int maximumNoteLength = 2000;

QString singleLine(QString text)
{
    text.replace(u'\r', u' ');
    text.replace(u'\n', u' ');
    return text.simplified();
}

QString markdownText(QString text)
{
    text.replace(u'\\', QStringLiteral("\\\\"));
    text.replace(u'*', QStringLiteral("\\*"));
    text.replace(u'_', QStringLiteral("\\_"));
    text.replace(u'`', QStringLiteral("\\`"));
    text.replace(u'[', QStringLiteral("\\["));
    text.replace(u']', QStringLiteral("\\]"));
    return text;
}

QString markdownQuote(const QString &text)
{
    QStringList lines = text.split(u'\n');
    for (QString &line : lines) {
        line.prepend(QStringLiteral("> "));
    }
    return lines.join(u'\n');
}

} // namespace

NotesCenterModel::NotesCenterModel(ProfileDatabase *database, QObject *parent)
    : QAbstractListModel(parent)
    , m_database(database)
{
    Q_ASSERT(m_database);
    refresh();
}

int NotesCenterModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visibleEntries.size();
}

QVariant NotesCenterModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleEntries.size()) {
        return {};
    }

    const LibraryReadingAnnotation &entry = m_visibleEntries.at(index.row());
    const ReadingAnnotation &annotation = entry.annotation;
    switch (role) {
    case IdRole:
        return annotation.id;
    case TypeRole:
        return annotation.type == ReadingAnnotationType::Bookmark
                   ? QStringLiteral("bookmark") : QStringLiteral("highlight");
    case SourceUrlRole:
        return entry.sourceUrl;
    case BookTitleRole:
        return entry.bookTitle;
    case BookAuthorRole:
        return entry.bookAuthor;
    case FormatNameRole:
        return entry.formatName;
    case ProgressRole:
        return annotation.progress;
    case PageRole:
        return annotation.page;
    case StartRole:
        return annotation.start;
    case LengthRole:
        return annotation.length;
    case LabelRole:
        return annotation.label;
    case ExcerptRole:
        return annotation.excerpt;
    case NoteRole:
        return annotation.note;
    case CreatedAtRole:
        return annotation.createdAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> NotesCenterModel::roleNames() const
{
    return {
        {IdRole, "annotationId"},
        {TypeRole, "annotationType"},
        {SourceUrlRole, "sourceUrl"},
        {BookTitleRole, "bookTitle"},
        {BookAuthorRole, "bookAuthor"},
        {FormatNameRole, "formatName"},
        {ProgressRole, "progress"},
        {PageRole, "page"},
        {StartRole, "start"},
        {LengthRole, "length"},
        {LabelRole, "label"},
        {ExcerptRole, "excerpt"},
        {NoteRole, "note"},
        {CreatedAtRole, "createdAt"}
    };
}

QString NotesCenterModel::query() const
{
    return m_query;
}

QString NotesCenterModel::typeFilter() const
{
    return m_typeFilter;
}

int NotesCenterModel::totalCount() const
{
    return m_allEntries.size();
}

int NotesCenterModel::bookmarkCount() const
{
    return m_bookmarkCount;
}

int NotesCenterModel::highlightCount() const
{
    return m_highlightCount;
}

int NotesCenterModel::noteCount() const
{
    return m_noteCount;
}

QString NotesCenterModel::errorMessage() const
{
    return m_errorMessage;
}

QUrl NotesCenterModel::lastExportUrl() const
{
    return m_lastExportUrl;
}

void NotesCenterModel::setQuery(const QString &query)
{
    const QString normalized = query.trimmed();
    if (m_query == normalized) {
        return;
    }
    m_query = normalized;
    emit queryChanged();
    applyFilter();
}

void NotesCenterModel::setTypeFilter(const QString &typeFilter)
{
    const QString normalized = normalizedTypeFilter(typeFilter);
    if (m_typeFilter == normalized) {
        return;
    }
    m_typeFilter = normalized;
    emit typeFilterChanged();
    applyFilter();
}

QVariantMap NotesCenterModel::get(int row) const
{
    return row >= 0 && row < m_visibleEntries.size()
               ? toVariantMap(m_visibleEntries.at(row)) : QVariantMap{};
}

QVariantMap NotesCenterModel::find(const QUrl &sourceUrl,
                                   const QString &annotationId) const
{
    for (const LibraryReadingAnnotation &entry : m_allEntries) {
        if (entry.sourceUrl == sourceUrl && entry.annotation.id == annotationId) {
            return toVariantMap(entry);
        }
    }
    return {};
}

bool NotesCenterModel::updateAnnotation(const QUrl &sourceUrl,
                                        const QString &annotationId,
                                        const QString &label,
                                        const QString &note)
{
    for (LibraryReadingAnnotation &entry : m_allEntries) {
        if (entry.sourceUrl != sourceUrl || entry.annotation.id != annotationId) {
            continue;
        }

        ReadingAnnotation updated = entry.annotation;
        updated.label = normalizedLabel(label);
        updated.note = normalizedNote(note);
        if (updated.label == entry.annotation.label && updated.note == entry.annotation.note) {
            return true;
        }
        if (!m_database->saveAnnotation(sourceUrl, updated)) {
            setErrorMessage(m_database->errorMessage());
            return false;
        }

        entry.annotation = updated;
        setErrorMessage({});
        updateCounts();
        applyFilter();
        emit annotationChanged(sourceUrl);
        emit profileChanged();
        return true;
    }

    setErrorMessage(tr("This reading mark no longer exists."));
    return false;
}

bool NotesCenterModel::removeAnnotation(const QUrl &sourceUrl,
                                        const QString &annotationId)
{
    const auto match = std::find_if(m_allEntries.cbegin(),
                                    m_allEntries.cend(),
                                    [&sourceUrl, &annotationId](const auto &entry) {
                                        return entry.sourceUrl == sourceUrl
                                               && entry.annotation.id == annotationId;
                                    });
    if (match == m_allEntries.cend()) {
        setErrorMessage(tr("This reading mark no longer exists."));
        return false;
    }
    if (!m_database->removeAnnotation(sourceUrl, annotationId)) {
        setErrorMessage(m_database->errorMessage());
        return false;
    }

    m_allEntries.erase(match);
    setErrorMessage({});
    updateCounts();
    applyFilter();
    emit annotationChanged(sourceUrl);
    emit profileChanged();
    return true;
}

bool NotesCenterModel::exportFiltered(const QUrl &fileUrl, const QString &format)
{
    if (!fileUrl.isLocalFile()) {
        setErrorMessage(tr("Choose a local export file."));
        return false;
    }

    const QString normalizedFormat = format.trimmed().toLower();
    if (normalizedFormat != QLatin1String("markdown")
        && normalizedFormat != QLatin1String("html")) {
        setErrorMessage(tr("Unsupported notes export format."));
        return false;
    }

    QSaveFile output(fileUrl.toLocalFile());
    if (!output.open(QIODevice::WriteOnly)) {
        setErrorMessage(tr("Could not create the notes export file."));
        return false;
    }
    const QByteArray content = normalizedFormat == QLatin1String("html")
                                   ? htmlExport() : markdownExport();
    if (output.write(content) != content.size() || !output.commit()) {
        setErrorMessage(tr("Could not save the notes export file."));
        return false;
    }

    setErrorMessage({});
    if (m_lastExportUrl != fileUrl) {
        m_lastExportUrl = fileUrl;
        emit lastExportUrlChanged();
    }
    emit exportCompleted(fileUrl, m_visibleEntries.size());
    return true;
}

void NotesCenterModel::refresh()
{
    beginResetModel();
    m_allEntries = m_database->libraryAnnotations();
    m_visibleEntries.clear();
    for (const LibraryReadingAnnotation &entry : m_allEntries) {
        if (matchesFilter(entry)) {
            m_visibleEntries.append(entry);
        }
    }
    endResetModel();
    updateCounts();
    emit countChanged();
}

void NotesCenterModel::clearError()
{
    setErrorMessage({});
}

QVariantMap NotesCenterModel::toVariantMap(const LibraryReadingAnnotation &entry)
{
    const ReadingAnnotation &annotation = entry.annotation;
    return {
        {QStringLiteral("annotationId"), annotation.id},
        {QStringLiteral("annotationType"),
         annotation.type == ReadingAnnotationType::Bookmark
             ? QStringLiteral("bookmark") : QStringLiteral("highlight")},
        {QStringLiteral("sourceUrl"), entry.sourceUrl},
        {QStringLiteral("bookTitle"), entry.bookTitle},
        {QStringLiteral("bookAuthor"), entry.bookAuthor},
        {QStringLiteral("formatName"), entry.formatName},
        {QStringLiteral("progress"), annotation.progress},
        {QStringLiteral("page"), annotation.page},
        {QStringLiteral("start"), annotation.start},
        {QStringLiteral("length"), annotation.length},
        {QStringLiteral("label"), annotation.label},
        {QStringLiteral("excerpt"), annotation.excerpt},
        {QStringLiteral("note"), annotation.note},
        {QStringLiteral("createdAt"), annotation.createdAt}
    };
}

QString NotesCenterModel::normalizedLabel(const QString &label)
{
    return singleLine(label).left(maximumLabelLength);
}

QString NotesCenterModel::normalizedNote(const QString &note)
{
    return note.trimmed().left(maximumNoteLength);
}

QString NotesCenterModel::normalizedTypeFilter(const QString &typeFilter)
{
    const QString normalized = typeFilter.trimmed().toLower();
    return normalized == QLatin1String("bookmarks")
                   || normalized == QLatin1String("highlights")
                   || normalized == QLatin1String("notes")
               ? normalized : QStringLiteral("all");
}

bool NotesCenterModel::matchesFilter(const LibraryReadingAnnotation &entry) const
{
    const ReadingAnnotation &annotation = entry.annotation;
    if (m_typeFilter == QLatin1String("bookmarks")
        && annotation.type != ReadingAnnotationType::Bookmark) {
        return false;
    }
    if (m_typeFilter == QLatin1String("highlights")
        && annotation.type != ReadingAnnotationType::Highlight) {
        return false;
    }
    if (m_typeFilter == QLatin1String("notes") && annotation.note.trimmed().isEmpty()) {
        return false;
    }
    if (m_query.isEmpty()) {
        return true;
    }

    const QStringList fields = {
        entry.bookTitle,
        entry.bookAuthor,
        entry.formatName,
        annotation.label,
        annotation.excerpt,
        annotation.note
    };
    return std::any_of(fields.cbegin(), fields.cend(), [this](const QString &field) {
        return field.contains(m_query, Qt::CaseInsensitive);
    });
}

QString NotesCenterModel::locationText(const ReadingAnnotation &annotation) const
{
    return annotation.page >= 0
               ? tr("Page %1").arg(annotation.page + 1)
               : tr("%1%").arg(qRound(annotation.progress * 100));
}

QByteArray NotesCenterModel::markdownExport() const
{
    QString output = QStringLiteral("# %1\n\n").arg(tr("SZHBooks reading notes"));
    output += tr("Exported %1").arg(
                  QLocale().toString(QDateTime::currentDateTime(), QLocale::ShortFormat));
    output += QStringLiteral("\n\n");

    for (const LibraryReadingAnnotation &entry : m_visibleEntries) {
        const ReadingAnnotation &annotation = entry.annotation;
        output += QStringLiteral("## %1\n\n").arg(markdownText(singleLine(entry.bookTitle)));
        if (!entry.bookAuthor.isEmpty()) {
            output += QStringLiteral("**%1:** %2  \n")
                          .arg(tr("Author"), markdownText(singleLine(entry.bookAuthor)));
        }
        output += QStringLiteral("**%1:** %2  \n")
                      .arg(tr("Location"), markdownText(locationText(annotation)));
        output += QStringLiteral("**%1:** %2\n\n")
                      .arg(tr("Type"),
                           annotation.type == ReadingAnnotationType::Bookmark
                               ? tr("Bookmark") : tr("Highlight"));
        if (!annotation.label.isEmpty()) {
            output += QStringLiteral("**%1**\n\n")
                          .arg(markdownText(annotation.label));
        }
        if (!annotation.excerpt.isEmpty()) {
            output += markdownQuote(annotation.excerpt) + QStringLiteral("\n\n");
        }
        if (!annotation.note.isEmpty()) {
            output += markdownText(annotation.note) + QStringLiteral("\n\n");
        }
        output += QStringLiteral("---\n\n");
    }
    return output.toUtf8();
}

QByteArray NotesCenterModel::htmlExport() const
{
    QString output = QStringLiteral(
        "<!doctype html><html><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<title>%1</title><style>body{max-width:760px;margin:40px auto;padding:0 24px;"
        "font:16px/1.55 system-ui,sans-serif;color:#171717;background:#fff}"
        "article{padding:22px 0;border-bottom:1px solid #ddd}h1,h2{line-height:1.2}"
        "h2{font-size:1.2rem;margin-bottom:6px}.meta{color:#666;font-size:.9rem}"
        "blockquote{margin:16px 0;padding:10px 16px;border-left:3px solid #555;"
        "background:#f4f4f4;white-space:pre-wrap}.note{white-space:pre-wrap}</style>"
        "</head><body><h1>%1</h1><p class=\"meta\">%2</p>")
                         .arg(tr("SZHBooks reading notes").toHtmlEscaped(),
                              tr("Exported %1")
                                  .arg(QLocale().toString(QDateTime::currentDateTime(),
                                                          QLocale::ShortFormat))
                                  .toHtmlEscaped());

    for (const LibraryReadingAnnotation &entry : m_visibleEntries) {
        const ReadingAnnotation &annotation = entry.annotation;
        output += QStringLiteral("<article><h2>%1</h2><p class=\"meta\">%2%3 · %4 · %5</p>")
                      .arg(singleLine(entry.bookTitle).toHtmlEscaped(),
                           entry.bookAuthor.isEmpty()
                               ? QString() : singleLine(entry.bookAuthor).toHtmlEscaped(),
                           entry.bookAuthor.isEmpty() ? QString() : QStringLiteral(" · "),
                           locationText(annotation).toHtmlEscaped(),
                           (annotation.type == ReadingAnnotationType::Bookmark
                                ? tr("Bookmark") : tr("Highlight")).toHtmlEscaped());
        if (!annotation.label.isEmpty()) {
            output += QStringLiteral("<p><strong>%1</strong></p>")
                          .arg(annotation.label.toHtmlEscaped());
        }
        if (!annotation.excerpt.isEmpty()) {
            output += QStringLiteral("<blockquote>%1</blockquote>")
                          .arg(annotation.excerpt.toHtmlEscaped());
        }
        if (!annotation.note.isEmpty()) {
            output += QStringLiteral("<p class=\"note\">%1</p>")
                          .arg(annotation.note.toHtmlEscaped());
        }
        output += QStringLiteral("</article>");
    }
    output += QStringLiteral("</body></html>");
    return output.toUtf8();
}

void NotesCenterModel::applyFilter()
{
    beginResetModel();
    m_visibleEntries.clear();
    for (const LibraryReadingAnnotation &entry : m_allEntries) {
        if (matchesFilter(entry)) {
            m_visibleEntries.append(entry);
        }
    }
    endResetModel();
    emit countChanged();
}

void NotesCenterModel::updateCounts()
{
    int bookmarks = 0;
    int highlights = 0;
    int notes = 0;
    for (const LibraryReadingAnnotation &entry : m_allEntries) {
        if (entry.annotation.type == ReadingAnnotationType::Bookmark) {
            ++bookmarks;
        } else {
            ++highlights;
        }
        if (!entry.annotation.note.trimmed().isEmpty()) {
            ++notes;
        }
    }
    if (m_bookmarkCount == bookmarks && m_highlightCount == highlights
        && m_noteCount == notes) {
        return;
    }
    m_bookmarkCount = bookmarks;
    m_highlightCount = highlights;
    m_noteCount = notes;
    emit countsChanged();
}

void NotesCenterModel::setErrorMessage(const QString &errorMessage)
{
    if (m_errorMessage == errorMessage) {
        return;
    }
    m_errorMessage = errorMessage;
    emit errorMessageChanged();
}
