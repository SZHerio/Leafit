#include "profiledatabase.h"

#include "documentstoragekey.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <algorithm>

namespace {

constexpr int schemaVersion = 4;
constexpr qreal minimumPdfScale = 0.4;
constexpr qreal maximumPdfScale = 3.0;

const QString documentsPrefix = QStringLiteral("documents/");
const QString annotationsPrefix = QStringLiteral("annotations/");
const QString lastBookKey = QStringLiteral("session/lastBookUrl");
const QString migrationKey = QStringLiteral("legacy_qsettings_migration");

QString serializedUrl(const QUrl &url)
{
    const QString value = url.toString(QUrl::FullyEncoded);
    return value.isNull() ? QStringLiteral("") : value;
}

QString storageString(QString value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QString storageString(const QVariant &value)
{
    return storageString(value.toString());
}

QString stringListToStorage(const QStringList &values)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(values))
                                 .toJson(QJsonDocument::Compact));
}

QStringList stringListFromStorage(const QString &value)
{
    const QJsonDocument document = QJsonDocument::fromJson(value.toUtf8());
    if (!document.isArray()) {
        return {};
    }
    QStringList result;
    for (const QJsonValue &item : document.array()) {
        if (item.isString()) {
            result.append(item.toString());
        }
    }
    return result;
}

QStringList stringListFromVariant(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QString>()) {
        return value.toString().split(u',', Qt::SkipEmptyParts);
    }
    if (value.metaType() == QMetaType::fromType<QStringList>()) {
        return value.toStringList();
    }
    QStringList result;
    for (const QVariant &item : value.toList()) {
        result.append(item.toString());
    }
    return result;
}

QString normalizedCollectionPath(QString collectionPath)
{
    collectionPath.replace(u'\\', u'/');
    collectionPath = QDir::cleanPath(collectionPath.trimmed());
    return collectionPath == QLatin1String(".")
               ? QStringLiteral("")
               : storageString(collectionPath);
}

QString normalizedTextReadingMode(const QString &readingMode)
{
    return readingMode.compare(QStringLiteral("pages"), Qt::CaseInsensitive) == 0
               ? QStringLiteral("pages")
               : QStringLiteral("scroll");
}

QString annotationTypeName(ReadingAnnotationType type)
{
    return type == ReadingAnnotationType::Highlight
               ? QStringLiteral("highlight")
               : QStringLiteral("bookmark");
}

ReadingAnnotationType annotationTypeFromName(const QString &name)
{
    return name == QLatin1String("highlight")
               ? ReadingAnnotationType::Highlight
               : ReadingAnnotationType::Bookmark;
}

QDateTime dateTimeFromStorage(const QString &value)
{
    QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(value, Qt::ISODate);
    }
    return dateTime;
}

ReadingAnnotation annotationFromQuery(const QSqlQuery &query, int offset = 0)
{
    ReadingAnnotation annotation;
    annotation.id = query.value(offset).toString();
    annotation.type = annotationTypeFromName(query.value(offset + 1).toString());
    annotation.progress = qBound(qreal(0), query.value(offset + 2).toReal(), qreal(1));
    annotation.page = query.value(offset + 3).toInt();
    annotation.start = query.value(offset + 4).toInt();
    annotation.length = qMax(0, query.value(offset + 5).toInt());
    annotation.label = query.value(offset + 6).toString();
    annotation.excerpt = query.value(offset + 7).toString();
    annotation.note = query.value(offset + 8).toString();
    annotation.createdAt = dateTimeFromStorage(query.value(offset + 9).toString());
    return annotation;
}

bool bindAndExecute(QSqlQuery *query,
                    const QList<QPair<QString, QVariant>> &bindings)
{
    for (const auto &binding : bindings) {
        query->bindValue(binding.first, binding.second);
    }
    return query->exec();
}

QString documentFieldKey(const QString &documentId, const QString &field)
{
    return documentsPrefix + documentId + u'/' + field;
}

QString annotationFieldKey(const QString &documentId,
                           const QString &annotationId,
                           const QString &field)
{
    return annotationsPrefix + documentId + u'/' + annotationId + u'/' + field;
}

} // namespace

ProfileDatabase::ProfileDatabase(const QString &databaseFilePath)
    : m_filePath(QFileInfo(databaseFilePath).absoluteFilePath())
    , m_connectionName(QStringLiteral("szhbooks-profile-%1").arg(
          QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
    if (!QDir().mkpath(QFileInfo(m_filePath).absolutePath())) {
        setLastError(QStringLiteral("Could not create the profile database directory."));
        return;
    }

    m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_database.setDatabaseName(m_filePath);
    if (!m_database.open()) {
        setLastError(m_database.lastError().text());
        return;
    }
    if (!initializeSchema()) {
        m_database.close();
    }
}

ProfileDatabase::~ProfileDatabase()
{
    if (m_database.isValid()) {
        m_database.close();
    }
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

QString ProfileDatabase::databasePathForSettings(const QString &settingsFilePath)
{
    QString overriddenPath = qEnvironmentVariable("SZHBOOKS_DATABASE_FILE");
    if (!overriddenPath.isEmpty()) {
        return QFileInfo(overriddenPath).absoluteFilePath();
    }
    const QFileInfo settingsInfo(settingsFilePath);
    const QString databaseName = settingsInfo.completeBaseName() == QLatin1String("settings")
                                     ? QStringLiteral("library.sqlite")
                                     : settingsInfo.completeBaseName() + QStringLiteral(".sqlite");
    return QDir(settingsInfo.absolutePath()).filePath(databaseName);
}

bool ProfileDatabase::isOpen() const
{
    return m_database.isOpen();
}

QString ProfileDatabase::filePath() const
{
    return m_filePath;
}

QString ProfileDatabase::errorMessage() const
{
    return m_errorMessage;
}

bool ProfileDatabase::initializeSchema()
{
    if (!execute(QStringLiteral("PRAGMA foreign_keys = ON"))
        || !execute(QStringLiteral("PRAGMA journal_mode = WAL"))
        || !execute(QStringLiteral("PRAGMA synchronous = NORMAL"))
        || !createSchema()) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM schema_meta WHERE key = 'version'"));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    const int storedVersion = query.next() ? query.value(0).toInt() : 0;
    if (storedVersion > schemaVersion) {
        setLastError(QStringLiteral("The profile database version is newer than this application."));
        return false;
    }

    QStringList migrations;
    if (storedVersion == 1) {
        migrations = {
            QStringLiteral("ALTER TABLE books ADD COLUMN series TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE books ADD COLUMN series_number REAL NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE books ADD COLUMN genres TEXT NOT NULL DEFAULT '[]'"),
            QStringLiteral("ALTER TABLE books ADD COLUMN tags TEXT NOT NULL DEFAULT '[]'"),
            QStringLiteral("ALTER TABLE books ADD COLUMN language TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE books ADD COLUMN publication_year INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE books ADD COLUMN custom_cover_url TEXT NOT NULL DEFAULT ''"),
            QStringLiteral("ALTER TABLE books ADD COLUMN metadata_edited INTEGER NOT NULL DEFAULT 0")
        };
    }
    if (storedVersion > 0 && storedVersion < 3) {
        migrations.append(
            QStringLiteral("ALTER TABLE books ADD COLUMN text_position INTEGER NOT NULL DEFAULT -1"));
        migrations.append(
            QStringLiteral("ALTER TABLE books ADD COLUMN text_reading_mode TEXT NOT NULL DEFAULT 'scroll'"));
    }

    const bool migrationTransaction = !migrations.isEmpty();
    if (migrationTransaction) {
        if (!m_database.transaction()) {
            setLastError(m_database.lastError().text());
            return false;
        }
        for (const QString &migration : migrations) {
            if (!execute(migration)) {
                m_database.rollback();
                return false;
            }
        }
    }

    QSqlQuery update(m_database);
    update.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO schema_meta(key, value) VALUES('version', :version)"));
    update.bindValue(QStringLiteral(":version"), schemaVersion);
    if (!update.exec()) {
        setLastError(update.lastError().text());
        if (migrationTransaction) {
            m_database.rollback();
        }
        return false;
    }
    if (migrationTransaction && !m_database.commit()) {
        setLastError(m_database.lastError().text());
        m_database.rollback();
        return false;
    }
    return true;
}

bool ProfileDatabase::createSchema()
{
    const QStringList statements = {
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS schema_meta("
            "key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS profile_state("
            "key TEXT PRIMARY KEY, value TEXT NOT NULL)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS books("
            "id TEXT PRIMARY KEY, source_url TEXT NOT NULL UNIQUE, "
            "in_library INTEGER NOT NULL DEFAULT 0, title TEXT NOT NULL DEFAULT '', "
            "author TEXT NOT NULL DEFAULT '', format_name TEXT NOT NULL DEFAULT '', "
            "collection_path TEXT NOT NULL DEFAULT '', "
            "metadata_fingerprint TEXT NOT NULL DEFAULT '', cover_url TEXT NOT NULL DEFAULT '', "
            "series TEXT NOT NULL DEFAULT '', series_number REAL NOT NULL DEFAULT 0, "
            "genres TEXT NOT NULL DEFAULT '[]', tags TEXT NOT NULL DEFAULT '[]', "
            "language TEXT NOT NULL DEFAULT '', publication_year INTEGER NOT NULL DEFAULT 0, "
            "custom_cover_url TEXT NOT NULL DEFAULT '', "
            "metadata_edited INTEGER NOT NULL DEFAULT 0, "
            "reading_progress REAL NOT NULL DEFAULT 0, text_progress REAL NOT NULL DEFAULT 0, "
            "text_position INTEGER NOT NULL DEFAULT -1, "
            "text_reading_mode TEXT NOT NULL DEFAULT 'scroll', "
            "pdf_page INTEGER NOT NULL DEFAULT 0, pdf_scale REAL NOT NULL DEFAULT 1, "
            "last_opened TEXT NOT NULL DEFAULT '')"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS annotations("
            "book_id TEXT NOT NULL, id TEXT NOT NULL, source_url TEXT NOT NULL, "
            "type TEXT NOT NULL, progress REAL NOT NULL DEFAULT 0, "
            "page INTEGER NOT NULL DEFAULT -1, start_position INTEGER NOT NULL DEFAULT -1, "
            "selection_length INTEGER NOT NULL DEFAULT 0, label TEXT NOT NULL DEFAULT '', "
            "excerpt TEXT NOT NULL DEFAULT '', note TEXT NOT NULL DEFAULT '', "
            "created_at TEXT NOT NULL DEFAULT '', PRIMARY KEY(book_id, id), "
            "FOREIGN KEY(book_id) REFERENCES books(id) ON UPDATE CASCADE ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE TABLE IF NOT EXISTS book_typography("
            "book_id TEXT PRIMARY KEY, reading_font TEXT NOT NULL, "
            "font_size INTEGER NOT NULL, line_height REAL NOT NULL, "
            "paragraph_spacing INTEGER NOT NULL, first_line_indent INTEGER NOT NULL, "
            "text_alignment TEXT NOT NULL, page_width INTEGER NOT NULL, "
            "FOREIGN KEY(book_id) REFERENCES books(id) ON UPDATE CASCADE ON DELETE CASCADE)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS books_library_order "
            "ON books(in_library, last_opened DESC, title)"),
        QStringLiteral(
            "CREATE INDEX IF NOT EXISTS annotations_book_order "
            "ON annotations(book_id, page, progress, created_at)")
    };

    for (const QString &statement : statements) {
        if (!execute(statement)) {
            return false;
        }
    }
    return true;
}

bool ProfileDatabase::migrateLegacySettings(QSettings *settings,
                                            QString *errorMessage)
{
    if (!settings || !isOpen()) {
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }

    QSqlQuery marker(m_database);
    marker.prepare(QStringLiteral("SELECT value FROM schema_meta WHERE key = :key"));
    marker.bindValue(QStringLiteral(":key"), migrationKey);
    if (!marker.exec()) {
        setLastError(marker.lastError().text());
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }
    const bool alreadyMigrated = marker.next() && marker.value(0).toInt() == 1;

    QVariantMap legacyValues;
    for (const QString &key : settings->allKeys()) {
        if (key.startsWith(documentsPrefix)
            || key.startsWith(annotationsPrefix)
            || key == lastBookKey) {
            legacyValues.insert(key, settings->value(key));
        }
    }

    if (!alreadyMigrated) {
        QVariantMap mergedValues = profileValues();
        for (auto value = legacyValues.cbegin(); value != legacyValues.cend(); ++value) {
            if (!mergedValues.contains(value.key())) {
                mergedValues.insert(value.key(), value.value());
            }
        }
        if (!replaceProfileValues(mergedValues, errorMessage)) {
            return false;
        }

        QSqlQuery writeMarker(m_database);
        writeMarker.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO schema_meta(key, value) VALUES(:key, '1')"));
        writeMarker.bindValue(QStringLiteral(":key"), migrationKey);
        if (!writeMarker.exec()) {
            setLastError(writeMarker.lastError().text());
            if (errorMessage) {
                *errorMessage = m_errorMessage;
            }
            return false;
        }
    }

    if (!legacyValues.isEmpty()) {
        settings->remove(QStringLiteral("documents"));
        settings->remove(QStringLiteral("annotations"));
        settings->remove(lastBookKey);
        settings->sync();
        if (settings->status() != QSettings::NoError) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Could not finish the legacy profile migration.");
            }
            return false;
        }
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

QVariantMap ProfileDatabase::profileValues() const
{
    QVariantMap values;
    if (!isOpen()) {
        return values;
    }

    QSqlQuery stateQuery(m_database);
    if (stateQuery.exec(QStringLiteral("SELECT key, value FROM profile_state"))) {
        while (stateQuery.next()) {
            values.insert(stateQuery.value(0).toString(), stateQuery.value(1));
        }
    }

    QSqlQuery bookQuery(m_database);
    if (!bookQuery.exec(QStringLiteral(
            "SELECT id, source_url, in_library, title, author, format_name, "
            "collection_path, metadata_fingerprint, cover_url, series, series_number, "
            "genres, tags, language, publication_year, custom_cover_url, metadata_edited, "
            "reading_progress, text_progress, text_position, text_reading_mode, "
            "pdf_page, pdf_scale, last_opened FROM books"))) {
        setLastError(bookQuery.lastError().text());
        return values;
    }
    while (bookQuery.next()) {
        const QString id = bookQuery.value(0).toString();
        values.insert(documentFieldKey(id, QStringLiteral("sourceUrl")), bookQuery.value(1));
        values.insert(documentFieldKey(id, QStringLiteral("inLibrary")),
                      bookQuery.value(2).toBool());
        values.insert(documentFieldKey(id, QStringLiteral("title")), bookQuery.value(3));
        values.insert(documentFieldKey(id, QStringLiteral("author")), bookQuery.value(4));
        values.insert(documentFieldKey(id, QStringLiteral("formatName")), bookQuery.value(5));
        values.insert(documentFieldKey(id, QStringLiteral("collectionPath")), bookQuery.value(6));
        values.insert(documentFieldKey(id, QStringLiteral("metadataFingerprint")),
                      bookQuery.value(7));
        if (!bookQuery.value(8).toString().isEmpty()) {
            values.insert(documentFieldKey(id, QStringLiteral("coverUrl")), bookQuery.value(8));
        }
        values.insert(documentFieldKey(id, QStringLiteral("series")), bookQuery.value(9));
        values.insert(documentFieldKey(id, QStringLiteral("seriesNumber")), bookQuery.value(10));
        values.insert(documentFieldKey(id, QStringLiteral("genres")),
                      stringListFromStorage(bookQuery.value(11).toString()));
        values.insert(documentFieldKey(id, QStringLiteral("tags")),
                      stringListFromStorage(bookQuery.value(12).toString()));
        values.insert(documentFieldKey(id, QStringLiteral("language")), bookQuery.value(13));
        values.insert(documentFieldKey(id, QStringLiteral("publicationYear")),
                      bookQuery.value(14));
        if (!bookQuery.value(15).toString().isEmpty()) {
            values.insert(documentFieldKey(id, QStringLiteral("customCoverUrl")),
                          bookQuery.value(15));
        }
        values.insert(documentFieldKey(id, QStringLiteral("metadataEdited")),
                      bookQuery.value(16).toBool());
        values.insert(documentFieldKey(id, QStringLiteral("readingProgress")),
                      bookQuery.value(17));
        values.insert(documentFieldKey(id, QStringLiteral("textProgress")), bookQuery.value(18));
        values.insert(documentFieldKey(id, QStringLiteral("textPosition")), bookQuery.value(19));
        values.insert(documentFieldKey(id, QStringLiteral("textReadingMode")),
                      normalizedTextReadingMode(bookQuery.value(20).toString()));
        values.insert(documentFieldKey(id, QStringLiteral("pdfPage")), bookQuery.value(21));
        values.insert(documentFieldKey(id, QStringLiteral("pdfScale")), bookQuery.value(22));
        if (!bookQuery.value(23).toString().isEmpty()) {
            values.insert(documentFieldKey(id, QStringLiteral("lastOpened")),
                          bookQuery.value(23));
        }
    }

    QSqlQuery typographyQuery(m_database);
    if (!typographyQuery.exec(QStringLiteral(
            "SELECT book_id, reading_font, font_size, line_height, paragraph_spacing, "
            "first_line_indent, text_alignment, page_width FROM book_typography"))) {
        setLastError(typographyQuery.lastError().text());
        return values;
    }
    while (typographyQuery.next()) {
        const QString id = typographyQuery.value(0).toString();
        values.insert(documentFieldKey(id, QStringLiteral("typography/enabled")), true);
        values.insert(documentFieldKey(id, QStringLiteral("typography/readingFont")),
                      typographyQuery.value(1));
        values.insert(documentFieldKey(id, QStringLiteral("typography/fontSize")),
                      typographyQuery.value(2));
        values.insert(documentFieldKey(id, QStringLiteral("typography/lineHeight")),
                      typographyQuery.value(3));
        values.insert(documentFieldKey(id, QStringLiteral("typography/paragraphSpacing")),
                      typographyQuery.value(4));
        values.insert(documentFieldKey(id, QStringLiteral("typography/firstLineIndent")),
                      typographyQuery.value(5));
        values.insert(documentFieldKey(id, QStringLiteral("typography/textAlignment")),
                      typographyQuery.value(6));
        values.insert(documentFieldKey(id, QStringLiteral("typography/pageWidth")),
                      typographyQuery.value(7));
    }

    QSqlQuery annotationQuery(m_database);
    if (!annotationQuery.exec(QStringLiteral(
            "SELECT book_id, id, source_url, type, progress, page, start_position, "
            "selection_length, label, excerpt, note, created_at FROM annotations"))) {
        setLastError(annotationQuery.lastError().text());
        return values;
    }
    while (annotationQuery.next()) {
        const QString bookId = annotationQuery.value(0).toString();
        const QString annotationId = annotationQuery.value(1).toString();
        values.insert(annotationsPrefix + bookId + QStringLiteral("/sourceUrl"),
                      annotationQuery.value(2));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("type")),
                      annotationQuery.value(3));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("progress")),
                      annotationQuery.value(4));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("page")),
                      annotationQuery.value(5));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("start")),
                      annotationQuery.value(6));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("length")),
                      annotationQuery.value(7));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("label")),
                      annotationQuery.value(8));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("excerpt")),
                      annotationQuery.value(9));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("note")),
                      annotationQuery.value(10));
        values.insert(annotationFieldKey(bookId, annotationId, QStringLiteral("createdAt")),
                      annotationQuery.value(11));
    }
    return values;
}

bool ProfileDatabase::replaceProfileValues(const QVariantMap &values,
                                           QString *errorMessage)
{
    if (!isOpen() || !m_database.transaction()) {
        setLastError(m_database.lastError().text());
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }

    if (!clearProfileData()) {
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        m_database.rollback();
        return false;
    }
    if (!importProfileValues(values, errorMessage)) {
        m_database.rollback();
        return false;
    }
    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        m_database.rollback();
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool ProfileDatabase::clearProfileData()
{
    return execute(QStringLiteral("DELETE FROM annotations"))
           && execute(QStringLiteral("DELETE FROM book_typography"))
           && execute(QStringLiteral("DELETE FROM books"))
           && execute(QStringLiteral("DELETE FROM profile_state"));
}

bool ProfileDatabase::importProfileValues(const QVariantMap &values,
                                          QString *errorMessage)
{
    QHash<QString, QVariantMap> books;
    QHash<QString, QVariantMap> annotationGroups;
    QVariantMap stateValues;

    for (auto value = values.cbegin(); value != values.cend(); ++value) {
        if (value.key() == lastBookKey) {
            stateValues.insert(value.key(), value.value());
            continue;
        }
        if (value.key().startsWith(documentsPrefix)) {
            const QString tail = value.key().mid(documentsPrefix.size());
            const qsizetype separator = tail.indexOf(u'/');
            if (separator > 0) {
                books[tail.left(separator)].insert(tail.mid(separator + 1), value.value());
            }
            continue;
        }
        if (value.key().startsWith(annotationsPrefix)) {
            const QString tail = value.key().mid(annotationsPrefix.size());
            const qsizetype separator = tail.indexOf(u'/');
            if (separator > 0) {
                annotationGroups[tail.left(separator)].insert(tail.mid(separator + 1),
                                                               value.value());
            }
        }
    }

    QSqlQuery stateQuery(m_database);
    stateQuery.prepare(QStringLiteral(
        "INSERT INTO profile_state(key, value) VALUES(:key, :value)"));
    for (auto value = stateValues.cbegin(); value != stateValues.cend(); ++value) {
        stateQuery.bindValue(QStringLiteral(":key"), value.key());
        stateQuery.bindValue(QStringLiteral(":value"), storageString(value.value()));
        if (!stateQuery.exec()) {
            setLastError(stateQuery.lastError().text());
            if (errorMessage) {
                *errorMessage = m_errorMessage;
            }
            return false;
        }
    }

    QSqlQuery bookQuery(m_database);
    bookQuery.prepare(QStringLiteral(
        "INSERT INTO books(id, source_url, in_library, title, author, format_name, "
        "collection_path, metadata_fingerprint, cover_url, series, series_number, "
        "genres, tags, language, publication_year, custom_cover_url, metadata_edited, "
        "reading_progress, text_progress, text_position, text_reading_mode, "
        "pdf_page, pdf_scale, last_opened) "
        "VALUES(:id, :source_url, :in_library, :title, :author, :format_name, "
        ":collection_path, :metadata_fingerprint, :cover_url, :series, :series_number, "
        ":genres, :tags, :language, :publication_year, :custom_cover_url, :metadata_edited, "
        ":reading_progress, :text_progress, :text_position, :text_reading_mode, "
        ":pdf_page, :pdf_scale, :last_opened)"));
    QSqlQuery typographyQuery(m_database);
    typographyQuery.prepare(QStringLiteral(
        "INSERT INTO book_typography(book_id, reading_font, font_size, line_height, "
        "paragraph_spacing, first_line_indent, text_alignment, page_width) "
        "VALUES(:book_id, :reading_font, :font_size, :line_height, "
        ":paragraph_spacing, :first_line_indent, :text_alignment, :page_width)"));
    for (auto book = books.cbegin(); book != books.cend(); ++book) {
        const QVariantMap data = book.value();
        const QString sourceUrl = data.value(QStringLiteral("sourceUrl")).toString();
        if (sourceUrl.isEmpty()) {
            continue;
        }
        const qreal textProgress = data.value(QStringLiteral("textProgress"), 0).toReal();
        const qreal readingProgress = data.value(QStringLiteral("readingProgress"),
                                                 textProgress).toReal();
        const QList<QPair<QString, QVariant>> bindings = {
            {QStringLiteral(":id"), book.key()},
            {QStringLiteral(":source_url"), sourceUrl},
            {QStringLiteral(":in_library"), data.value(QStringLiteral("inLibrary"), false).toBool()},
            {QStringLiteral(":title"), storageString(data.value(QStringLiteral("title")))},
            {QStringLiteral(":author"), storageString(data.value(QStringLiteral("author")))},
            {QStringLiteral(":format_name"), storageString(data.value(QStringLiteral("formatName")))},
            {QStringLiteral(":collection_path"),
             normalizedCollectionPath(data.value(QStringLiteral("collectionPath")).toString())},
            {QStringLiteral(":metadata_fingerprint"),
             storageString(data.value(QStringLiteral("metadataFingerprint")))},
            {QStringLiteral(":cover_url"), storageString(data.value(QStringLiteral("coverUrl")))},
            {QStringLiteral(":series"), storageString(data.value(QStringLiteral("series")))},
            {QStringLiteral(":series_number"),
             qMax(qreal(0), data.value(QStringLiteral("seriesNumber"), 0).toReal())},
            {QStringLiteral(":genres"),
             stringListToStorage(stringListFromVariant(data.value(QStringLiteral("genres"))))},
            {QStringLiteral(":tags"),
             stringListToStorage(stringListFromVariant(data.value(QStringLiteral("tags"))))},
            {QStringLiteral(":language"), storageString(data.value(QStringLiteral("language")))},
            {QStringLiteral(":publication_year"),
             qBound(0, data.value(QStringLiteral("publicationYear"), 0).toInt(), 9999)},
            {QStringLiteral(":custom_cover_url"),
             storageString(data.value(QStringLiteral("customCoverUrl")))},
            {QStringLiteral(":metadata_edited"),
             data.value(QStringLiteral("metadataEdited"), false).toBool()},
            {QStringLiteral(":reading_progress"), qBound(qreal(0), readingProgress, qreal(1))},
            {QStringLiteral(":text_progress"), qBound(qreal(0), textProgress, qreal(1))},
            {QStringLiteral(":text_position"),
             qMax(-1, data.value(QStringLiteral("textPosition"), -1).toInt())},
            {QStringLiteral(":text_reading_mode"),
             normalizedTextReadingMode(
                 data.value(QStringLiteral("textReadingMode")).toString())},
            {QStringLiteral(":pdf_page"), qMax(0, data.value(QStringLiteral("pdfPage"), 0).toInt())},
            {QStringLiteral(":pdf_scale"),
             qBound(minimumPdfScale,
                    data.value(QStringLiteral("pdfScale"), 1).toReal(),
                    maximumPdfScale)},
            {QStringLiteral(":last_opened"), storageString(data.value(QStringLiteral("lastOpened")))}
        };
        if (!bindAndExecute(&bookQuery, bindings)) {
            setLastError(bookQuery.lastError().text());
            if (errorMessage) {
                *errorMessage = m_errorMessage;
            }
            return false;
        }

        if (data.value(QStringLiteral("typography/enabled"), false).toBool()) {
            const ReadingTypography defaults;
            const ReadingTypography typography = ReadingTypography::fromVariantMap({
                {QStringLiteral("readingFont"),
                 data.value(QStringLiteral("typography/readingFont"),
                            defaults.readingFont)},
                {QStringLiteral("fontSize"),
                 data.value(QStringLiteral("typography/fontSize"), defaults.fontSize)},
                {QStringLiteral("lineHeight"),
                 data.value(QStringLiteral("typography/lineHeight"), defaults.lineHeight)},
                {QStringLiteral("paragraphSpacing"),
                 data.value(QStringLiteral("typography/paragraphSpacing"),
                            defaults.paragraphSpacing)},
                {QStringLiteral("firstLineIndent"),
                 data.value(QStringLiteral("typography/firstLineIndent"),
                            defaults.firstLineIndent)},
                {QStringLiteral("textAlignment"),
                 data.value(QStringLiteral("typography/textAlignment"),
                            defaults.textAlignment)},
                {QStringLiteral("pageWidth"),
                 data.value(QStringLiteral("typography/pageWidth"), defaults.pageWidth)}
            });
            const QList<QPair<QString, QVariant>> typographyBindings = {
                {QStringLiteral(":book_id"), book.key()},
                {QStringLiteral(":reading_font"), typography.readingFont},
                {QStringLiteral(":font_size"), typography.fontSize},
                {QStringLiteral(":line_height"), typography.lineHeight},
                {QStringLiteral(":paragraph_spacing"), typography.paragraphSpacing},
                {QStringLiteral(":first_line_indent"), typography.firstLineIndent},
                {QStringLiteral(":text_alignment"), typography.textAlignment},
                {QStringLiteral(":page_width"), typography.pageWidth}
            };
            if (!bindAndExecute(&typographyQuery, typographyBindings)) {
                setLastError(typographyQuery.lastError().text());
                if (errorMessage) {
                    *errorMessage = m_errorMessage;
                }
                return false;
            }
        }
    }

    QSqlQuery annotationQuery(m_database);
    annotationQuery.prepare(QStringLiteral(
        "INSERT INTO annotations(book_id, id, source_url, type, progress, page, "
        "start_position, selection_length, label, excerpt, note, created_at) "
        "VALUES(:book_id, :id, :source_url, :type, :progress, :page, :start, "
        ":length, :label, :excerpt, :note, :created_at)"));
    for (auto group = annotationGroups.cbegin(); group != annotationGroups.cend(); ++group) {
        const QVariantMap groupData = group.value();
        const QString sourceUrl = groupData.value(QStringLiteral("sourceUrl")).toString();
        if (sourceUrl.isEmpty()) {
            continue;
        }
        if (!books.contains(group.key())) {
            QSqlQuery ensure(m_database);
            ensure.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO books(id, source_url) VALUES(:id, :source_url)"));
            ensure.bindValue(QStringLiteral(":id"), group.key());
            ensure.bindValue(QStringLiteral(":source_url"), sourceUrl);
            if (!ensure.exec()) {
                setLastError(ensure.lastError().text());
                if (errorMessage) {
                    *errorMessage = m_errorMessage;
                }
                return false;
            }
        }

        QHash<QString, QVariantMap> annotations;
        for (auto value = groupData.cbegin(); value != groupData.cend(); ++value) {
            const qsizetype separator = value.key().indexOf(u'/');
            if (separator > 0) {
                annotations[value.key().left(separator)].insert(
                    value.key().mid(separator + 1), value.value());
            }
        }
        for (auto annotation = annotations.cbegin(); annotation != annotations.cend(); ++annotation) {
            const QVariantMap data = annotation.value();
            const QList<QPair<QString, QVariant>> bindings = {
                {QStringLiteral(":book_id"), group.key()},
                {QStringLiteral(":id"), annotation.key()},
                {QStringLiteral(":source_url"), sourceUrl},
                {QStringLiteral(":type"), storageString(data.value(QStringLiteral("type"), QStringLiteral("bookmark")))},
                {QStringLiteral(":progress"), qBound(qreal(0), data.value(QStringLiteral("progress"), 0).toReal(), qreal(1))},
                {QStringLiteral(":page"), data.value(QStringLiteral("page"), -1)},
                {QStringLiteral(":start"), data.value(QStringLiteral("start"), -1)},
                {QStringLiteral(":length"), qMax(0, data.value(QStringLiteral("length"), 0).toInt())},
                {QStringLiteral(":label"), storageString(data.value(QStringLiteral("label")))},
                {QStringLiteral(":excerpt"), storageString(data.value(QStringLiteral("excerpt")))},
                {QStringLiteral(":note"), storageString(data.value(QStringLiteral("note")))},
                {QStringLiteral(":created_at"), storageString(data.value(QStringLiteral("createdAt")))}
            };
            if (!bindAndExecute(&annotationQuery, bindings)) {
                setLastError(annotationQuery.lastError().text());
                if (errorMessage) {
                    *errorMessage = m_errorMessage;
                }
                return false;
            }
        }
    }
    return true;
}

QUrl ProfileDatabase::lastBookUrl() const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT value FROM profile_state WHERE key = :key"));
    query.bindValue(QStringLiteral(":key"), lastBookKey);
    return query.exec() && query.next() ? QUrl(query.value(0).toString()) : QUrl();
}

bool ProfileDatabase::setLastBookUrl(const QUrl &documentUrl)
{
    QSqlQuery query(m_database);
    if (documentUrl.isEmpty()) {
        query.prepare(QStringLiteral("DELETE FROM profile_state WHERE key = :key"));
        query.bindValue(QStringLiteral(":key"), lastBookKey);
    } else {
        query.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO profile_state(key, value) VALUES(:key, :value)"));
        query.bindValue(QStringLiteral(":key"), lastBookKey);
        query.bindValue(QStringLiteral(":value"), serializedUrl(documentUrl));
    }
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::ensureBookRecord(const QUrl &documentUrl)
{
    if (documentUrl.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO books(id, source_url) VALUES(:id, :source_url)"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    query.bindValue(QStringLiteral(":source_url"), serializedUrl(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

qreal ProfileDatabase::textPosition(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT text_progress FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next()
               ? qBound(qreal(0), query.value(0).toReal(), qreal(1))
               : qreal(0);
}

int ProfileDatabase::textCharacterPosition(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT text_position FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next() ? qMax(-1, query.value(0).toInt()) : -1;
}

QString ProfileDatabase::textReadingMode(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT text_reading_mode FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next()
               ? normalizedTextReadingMode(query.value(0).toString())
               : QStringLiteral("scroll");
}

std::optional<ReadingTypography> ProfileDatabase::bookTypography(
    const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT reading_font, font_size, line_height, paragraph_spacing, "
        "first_line_indent, text_alignment, page_width "
        "FROM book_typography WHERE book_id = :book_id"));
    query.bindValue(QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    return ReadingTypography::fromVariantMap({
        {QStringLiteral("readingFont"), query.value(0)},
        {QStringLiteral("fontSize"), query.value(1)},
        {QStringLiteral("lineHeight"), query.value(2)},
        {QStringLiteral("paragraphSpacing"), query.value(3)},
        {QStringLiteral("firstLineIndent"), query.value(4)},
        {QStringLiteral("textAlignment"), query.value(5)},
        {QStringLiteral("pageWidth"), query.value(6)}
    });
}

int ProfileDatabase::pdfPage(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT pdf_page FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next() ? qMax(0, query.value(0).toInt()) : 0;
}

qreal ProfileDatabase::pdfScale(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT pdf_scale FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next()
               ? qBound(minimumPdfScale, query.value(0).toReal(), maximumPdfScale)
               : qreal(1);
}

bool ProfileDatabase::saveTextPosition(const QUrl &documentUrl,
                                       qreal progress,
                                       int characterPosition)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    progress = qBound(qreal(0), progress, qreal(1));
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET text_progress = :progress, reading_progress = :progress, "
        "text_position = :position "
        "WHERE id = :id"));
    query.bindValue(QStringLiteral(":progress"), progress);
    query.bindValue(QStringLiteral(":position"), qMax(-1, characterPosition));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::setTextReadingMode(const QUrl &documentUrl,
                                         const QString &readingMode)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET text_reading_mode = :mode WHERE id = :id"));
    query.bindValue(QStringLiteral(":mode"), normalizedTextReadingMode(readingMode));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::setBookTypography(const QUrl &documentUrl,
                                        const ReadingTypography &typography)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO book_typography(book_id, reading_font, font_size, line_height, "
        "paragraph_spacing, first_line_indent, text_alignment, page_width) "
        "VALUES(:book_id, :reading_font, :font_size, :line_height, "
        ":paragraph_spacing, :first_line_indent, :text_alignment, :page_width) "
        "ON CONFLICT(book_id) DO UPDATE SET reading_font = excluded.reading_font, "
        "font_size = excluded.font_size, line_height = excluded.line_height, "
        "paragraph_spacing = excluded.paragraph_spacing, "
        "first_line_indent = excluded.first_line_indent, "
        "text_alignment = excluded.text_alignment, page_width = excluded.page_width"));
    const QList<QPair<QString, QVariant>> bindings = {
        {QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl)},
        {QStringLiteral(":reading_font"), typography.readingFont},
        {QStringLiteral(":font_size"), typography.fontSize},
        {QStringLiteral(":line_height"), typography.lineHeight},
        {QStringLiteral(":paragraph_spacing"), typography.paragraphSpacing},
        {QStringLiteral(":first_line_indent"), typography.firstLineIndent},
        {QStringLiteral(":text_alignment"), typography.textAlignment},
        {QStringLiteral(":page_width"), typography.pageWidth}
    };
    if (!bindAndExecute(&query, bindings)) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::clearBookTypography(const QUrl &documentUrl)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "DELETE FROM book_typography WHERE book_id = :book_id"));
    query.bindValue(QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::savePdfPosition(const QUrl &documentUrl,
                                      int page,
                                      qreal scale,
                                      qreal progress)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET pdf_page = :page, pdf_scale = :scale, "
        "reading_progress = :progress WHERE id = :id"));
    query.bindValue(QStringLiteral(":page"), qMax(0, page));
    query.bindValue(QStringLiteral(":scale"),
                    qBound(minimumPdfScale, scale, maximumPdfScale));
    query.bindValue(QStringLiteral(":progress"),
                    qBound(qreal(0), progress, qreal(1)));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

QVector<LibraryBook> ProfileDatabase::libraryBooks() const
{
    QVector<LibraryBook> books;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "SELECT source_url, title, author, series, series_number, genres, tags, "
            "language, publication_year, format_name, collection_path, "
            "metadata_fingerprint, cover_url, custom_cover_url, metadata_edited, "
            "reading_progress, last_opened "
            "FROM books WHERE in_library = 1 "
            "ORDER BY last_opened DESC, title COLLATE NOCASE"))) {
        setLastError(query.lastError().text());
        return books;
    }
    while (query.next()) {
        LibraryBook book;
        book.sourceUrl = QUrl(query.value(0).toString());
        if (book.sourceUrl.isEmpty()) {
            continue;
        }
        const QFileInfo fileInfo(book.sourceUrl.toLocalFile());
        book.sourcePath = book.sourceUrl.isLocalFile()
                              ? fileInfo.absoluteFilePath()
                              : book.sourceUrl.toDisplayString();
        book.title = query.value(1).toString().trimmed();
        if (book.title.isEmpty()) {
            book.title = fileInfo.completeBaseName();
        }
        book.author = query.value(2).toString().trimmed();
        book.series = query.value(3).toString().trimmed();
        book.seriesNumber = qMax(qreal(0), query.value(4).toReal());
        book.genres = stringListFromStorage(query.value(5).toString());
        book.tags = stringListFromStorage(query.value(6).toString());
        book.language = query.value(7).toString().trimmed();
        book.publicationYear = qBound(0, query.value(8).toInt(), 9999);
        book.formatName = query.value(9).toString().trimmed();
        if (book.formatName.isEmpty()) {
            book.formatName = fileInfo.suffix().toUpper();
        }
        book.collectionPath = normalizedCollectionPath(query.value(10).toString());
        book.metadataFingerprint = query.value(11).toString();
        const QUrl automaticCoverUrl(query.value(12).toString());
        book.customCoverUrl = QUrl(query.value(13).toString());
        const bool customCoverAvailable = !book.customCoverUrl.isEmpty()
                                          && (!book.customCoverUrl.isLocalFile()
                                              || QFileInfo::exists(
                                                  book.customCoverUrl.toLocalFile()));
        book.coverUrl = customCoverAvailable ? book.customCoverUrl : automaticCoverUrl;
        book.metadataEdited = query.value(14).toBool();
        book.progress = qBound(qreal(0), query.value(15).toReal(), qreal(1));
        book.lastOpened = dateTimeFromStorage(query.value(16).toString());
        book.fileAvailable = !book.sourceUrl.isLocalFile() || fileInfo.exists();
        books.append(book);
    }
    return books;
}

bool ProfileDatabase::recordBookOpened(const QUrl &documentUrl,
                                       const QString &title,
                                       const QString &author,
                                       const QString &formatName)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET in_library = 1, "
        "title = CASE WHEN metadata_edited = 1 THEN title ELSE :title END, "
        "author = CASE WHEN metadata_edited = 1 THEN author ELSE :author END, "
        "format_name = :format_name, last_opened = :last_opened WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), storageString(title.trimmed()));
    query.bindValue(QStringLiteral(":author"), storageString(author.trimmed()));
    query.bindValue(QStringLiteral(":format_name"), storageString(formatName.trimmed()));
    query.bindValue(QStringLiteral(":last_opened"),
                    QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::registerLibraryBook(const QUrl &documentUrl,
                                          const QString &title,
                                          const QString &author,
                                          const QString &formatName,
                                          const QUrl &coverUrl,
                                          const QString &metadataFingerprint,
                                          const QString &collectionPath)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET in_library = 1, title = :title, author = :author, "
        "format_name = :format_name, collection_path = :collection_path, "
        "metadata_fingerprint = :fingerprint, cover_url = :cover_url WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), storageString(title.trimmed()));
    query.bindValue(QStringLiteral(":author"), storageString(author.trimmed()));
    query.bindValue(QStringLiteral(":format_name"), storageString(formatName.trimmed()));
    query.bindValue(QStringLiteral(":collection_path"),
                    normalizedCollectionPath(collectionPath));
    query.bindValue(QStringLiteral(":fingerprint"), storageString(metadataFingerprint));
    query.bindValue(QStringLiteral(":cover_url"), serializedUrl(coverUrl));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::updateBookMetadata(const QUrl &documentUrl,
                                         const QString &title,
                                         const QString &author,
                                         const QString &formatName,
                                         const QUrl &coverUrl,
                                         const QString &metadataFingerprint)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET "
        "title = CASE WHEN metadata_edited = 1 THEN title ELSE :title END, "
        "author = CASE WHEN metadata_edited = 1 THEN author ELSE :author END, "
        "format_name = :format_name, "
        "cover_url = :cover_url, metadata_fingerprint = :fingerprint WHERE id = :id"));
    query.bindValue(QStringLiteral(":title"), storageString(title.trimmed()));
    query.bindValue(QStringLiteral(":author"), storageString(author.trimmed()));
    query.bindValue(QStringLiteral(":format_name"), storageString(formatName.trimmed()));
    query.bindValue(QStringLiteral(":cover_url"), serializedUrl(coverUrl));
    query.bindValue(QStringLiteral(":fingerprint"), storageString(metadataFingerprint));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::updateBookDetails(const QVector<QUrl> &documentUrls,
                                        const BookMetadataPatch &patch,
                                        QString *errorMessage)
{
    if (documentUrls.isEmpty() || patch.isEmpty()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return true;
    }
    if (!m_database.transaction()) {
        setLastError(m_database.lastError().text());
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET "
        "title = CASE WHEN :has_title THEN :title ELSE title END, "
        "author = CASE WHEN :has_author THEN :author ELSE author END, "
        "series = CASE WHEN :has_series THEN :series ELSE series END, "
        "series_number = CASE WHEN :has_series_number THEN :series_number ELSE series_number END, "
        "genres = CASE WHEN :has_genres THEN :genres ELSE genres END, "
        "tags = CASE WHEN :has_tags THEN :tags ELSE tags END, "
        "language = CASE WHEN :has_language THEN :language ELSE language END, "
        "publication_year = CASE WHEN :has_year THEN :publication_year ELSE publication_year END, "
        "custom_cover_url = CASE WHEN :has_custom_cover THEN :custom_cover_url ELSE custom_cover_url END, "
        "metadata_edited = CASE WHEN :lock_metadata THEN 1 ELSE metadata_edited END "
        "WHERE id = :id AND in_library = 1"));

    query.bindValue(QStringLiteral(":has_title"), patch.title.has_value());
    query.bindValue(QStringLiteral(":title"),
                    storageString(patch.title.value_or(QStringLiteral(""))));
    query.bindValue(QStringLiteral(":has_author"), patch.author.has_value());
    query.bindValue(QStringLiteral(":author"),
                    storageString(patch.author.value_or(QStringLiteral(""))));
    query.bindValue(QStringLiteral(":has_series"), patch.series.has_value());
    query.bindValue(QStringLiteral(":series"),
                    storageString(patch.series.value_or(QStringLiteral(""))));
    query.bindValue(QStringLiteral(":has_series_number"), patch.seriesNumber.has_value());
    query.bindValue(QStringLiteral(":series_number"), patch.seriesNumber.value_or(0));
    query.bindValue(QStringLiteral(":has_genres"), patch.genres.has_value());
    query.bindValue(QStringLiteral(":genres"),
                    stringListToStorage(patch.genres.value_or(QStringList())));
    query.bindValue(QStringLiteral(":has_tags"), patch.tags.has_value());
    query.bindValue(QStringLiteral(":tags"),
                    stringListToStorage(patch.tags.value_or(QStringList())));
    query.bindValue(QStringLiteral(":has_language"), patch.language.has_value());
    query.bindValue(QStringLiteral(":language"),
                    storageString(patch.language.value_or(QStringLiteral(""))));
    query.bindValue(QStringLiteral(":has_year"), patch.publicationYear.has_value());
    query.bindValue(QStringLiteral(":publication_year"), patch.publicationYear.value_or(0));
    query.bindValue(QStringLiteral(":has_custom_cover"), patch.customCoverUrl.has_value());
    query.bindValue(QStringLiteral(":custom_cover_url"),
                    serializedUrl(patch.customCoverUrl.value_or(QUrl())));
    query.bindValue(QStringLiteral(":lock_metadata"),
                    patch.title.has_value() || patch.author.has_value());

    for (const QUrl &documentUrl : documentUrls) {
        query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
        if (!query.exec() || query.numRowsAffected() == 0) {
            setLastError(query.lastError().text().isEmpty()
                             ? QStringLiteral("A selected book is no longer in the library.")
                             : query.lastError().text());
            m_database.rollback();
            if (errorMessage) {
                *errorMessage = m_errorMessage;
            }
            return false;
        }
    }
    if (!m_database.commit()) {
        setLastError(m_database.lastError().text());
        m_database.rollback();
        if (errorMessage) {
            *errorMessage = m_errorMessage;
        }
        return false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool ProfileDatabase::containsLibraryBook(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT in_library FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next() && query.value(0).toBool();
}

bool ProfileDatabase::hasLibraryRecord(const QUrl &documentUrl) const
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("SELECT 1 FROM books WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    return query.exec() && query.next();
}

bool ProfileDatabase::setBookCollection(const QUrl &documentUrl,
                                        const QString &collectionPath)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET collection_path = :collection_path "
        "WHERE id = :id AND in_library = 1 AND collection_path <> :collection_path"));
    query.bindValue(QStringLiteral(":collection_path"),
                    normalizedCollectionPath(collectionPath));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool ProfileDatabase::removeFromLibrary(const QUrl &documentUrl)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("UPDATE books SET in_library = 0 WHERE id = :id"));
    query.bindValue(QStringLiteral(":id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool ProfileDatabase::relinkDocument(const QUrl &oldDocumentUrl,
                                     const QUrl &newDocumentUrl)
{
    const QString oldId = DocumentStorageKey::id(oldDocumentUrl);
    const QString newId = DocumentStorageKey::id(newDocumentUrl);
    if (oldDocumentUrl.isEmpty() || newDocumentUrl.isEmpty()
        || (oldId != newId && containsLibraryBook(newDocumentUrl))
        || !m_database.transaction()) {
        return false;
    }

    if (oldId != newId && hasLibraryRecord(newDocumentUrl)) {
        QSqlQuery removeDestination(m_database);
        removeDestination.prepare(QStringLiteral(
            "DELETE FROM books WHERE id = :id AND in_library = 0"));
        removeDestination.bindValue(QStringLiteral(":id"), newId);
        if (!removeDestination.exec()) {
            setLastError(removeDestination.lastError().text());
            m_database.rollback();
            return false;
        }
    }

    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE books SET id = :new_id, source_url = :source_url, "
        "metadata_fingerprint = '', cover_url = '' "
        "WHERE id = :old_id AND in_library = 1"));
    query.bindValue(QStringLiteral(":new_id"), newId);
    query.bindValue(QStringLiteral(":source_url"), serializedUrl(newDocumentUrl));
    query.bindValue(QStringLiteral(":old_id"), oldId);
    if (!query.exec() || query.numRowsAffected() == 0) {
        setLastError(query.lastError().text());
        m_database.rollback();
        return false;
    }

    QSqlQuery annotationsQuery(m_database);
    annotationsQuery.prepare(QStringLiteral(
        "UPDATE annotations SET source_url = :source_url WHERE book_id = :book_id"));
    annotationsQuery.bindValue(QStringLiteral(":source_url"), serializedUrl(newDocumentUrl));
    annotationsQuery.bindValue(QStringLiteral(":book_id"), newId);
    if (!annotationsQuery.exec() || !m_database.commit()) {
        setLastError(annotationsQuery.lastError().text().isEmpty()
                         ? m_database.lastError().text()
                         : annotationsQuery.lastError().text());
        m_database.rollback();
        return false;
    }
    return true;
}

QVector<ReadingAnnotation> ProfileDatabase::annotations(const QUrl &documentUrl) const
{
    QVector<ReadingAnnotation> annotations;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT id, type, progress, page, start_position, selection_length, label, "
        "excerpt, note, created_at FROM annotations WHERE book_id = :book_id "
        "ORDER BY CASE WHEN page >= 0 THEN page ELSE 2147483647 END, progress, created_at"));
    query.bindValue(QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return annotations;
    }
    while (query.next()) {
        annotations.append(annotationFromQuery(query));
    }
    return annotations;
}

QVector<LibraryReadingAnnotation> ProfileDatabase::libraryAnnotations() const
{
    QVector<LibraryReadingAnnotation> annotations;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "SELECT b.source_url, b.title, b.author, b.format_name, "
        "a.id, a.type, a.progress, a.page, a.start_position, a.selection_length, "
        "a.label, a.excerpt, a.note, a.created_at "
        "FROM annotations a JOIN books b ON b.id = a.book_id "
        "WHERE b.in_library = 1 ORDER BY a.created_at DESC, b.title, a.progress"));
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return annotations;
    }

    while (query.next()) {
        LibraryReadingAnnotation entry;
        entry.sourceUrl = QUrl(query.value(0).toString());
        entry.bookTitle = query.value(1).toString();
        entry.bookAuthor = query.value(2).toString();
        entry.formatName = query.value(3).toString();
        entry.annotation = annotationFromQuery(query, 4);
        annotations.append(entry);
    }
    return annotations;
}

bool ProfileDatabase::saveAnnotation(const QUrl &documentUrl,
                                     const ReadingAnnotation &annotation)
{
    if (!ensureBookRecord(documentUrl)) {
        return false;
    }
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO annotations(book_id, id, source_url, type, progress, "
        "page, start_position, selection_length, label, excerpt, note, created_at) "
        "VALUES(:book_id, :id, :source_url, :type, :progress, :page, :start, "
        ":length, :label, :excerpt, :note, :created_at)"));
    const QList<QPair<QString, QVariant>> bindings = {
        {QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl)},
        {QStringLiteral(":id"), annotation.id},
        {QStringLiteral(":source_url"), serializedUrl(documentUrl)},
        {QStringLiteral(":type"), annotationTypeName(annotation.type)},
        {QStringLiteral(":progress"), qBound(qreal(0), annotation.progress, qreal(1))},
        {QStringLiteral(":page"), annotation.page},
        {QStringLiteral(":start"), annotation.start},
        {QStringLiteral(":length"), qMax(0, annotation.length)},
        {QStringLiteral(":label"), storageString(annotation.label)},
        {QStringLiteral(":excerpt"), storageString(annotation.excerpt)},
        {QStringLiteral(":note"), storageString(annotation.note)},
        {QStringLiteral(":created_at"), storageString(annotation.createdAt.toString(Qt::ISODateWithMs))}
    };
    if (!bindAndExecute(&query, bindings)) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

bool ProfileDatabase::removeAnnotation(const QUrl &documentUrl,
                                       const QString &annotationId)
{
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "DELETE FROM annotations WHERE book_id = :book_id AND id = :id"));
    query.bindValue(QStringLiteral(":book_id"), DocumentStorageKey::id(documentUrl));
    query.bindValue(QStringLiteral(":id"), annotationId);
    if (!query.exec()) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

void ProfileDatabase::sync()
{
    execute(QStringLiteral("PRAGMA wal_checkpoint(PASSIVE)"));
}

bool ProfileDatabase::execute(const QString &statement) const
{
    QSqlQuery query(m_database);
    if (!query.exec(statement)) {
        setLastError(query.lastError().text());
        return false;
    }
    return true;
}

void ProfileDatabase::setLastError(const QString &message) const
{
    m_errorMessage = message.trimmed();
}
