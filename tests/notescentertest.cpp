#include "../notes/notescentermodel.h"
#include "../storage/localstatestore.h"
#include "../storage/readingannotationstore.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class NotesCenterTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersEditsAndRemoves();
    void exportsMarkdownAndHtml();
};

void NotesCenterTest::filtersEditsAndRemoves()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalStateStore state(directory.filePath(QStringLiteral("settings.ini")));
    const QUrl firstBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("alpha.txt")));
    const QUrl secondBook = QUrl::fromLocalFile(directory.filePath(QStringLiteral("beta.epub")));
    state.registerLibraryBook(firstBook,
                              QStringLiteral("Alpha book"),
                              QStringLiteral("Alice"),
                              QStringLiteral("TXT"),
                              {}, {}, {});
    state.registerLibraryBook(secondBook,
                              QStringLiteral("Beta book"),
                              QStringLiteral("Bob"),
                              QStringLiteral("EPUB"),
                              {}, {}, {});

    ReadingAnnotationStore annotations(state.profileDatabase());
    annotations.setDocumentUrl(firstBook);
    QVERIFY(annotations.toggleBookmark(0.25, -1, QStringLiteral("Opening")));
    const QString firstHighlight = annotations.addHighlight(
        12, 18, QStringLiteral("A useful excerpt"), QStringLiteral("First note"), 0.3, -1);
    QVERIFY(!firstHighlight.isEmpty());
    annotations.setDocumentUrl(secondBook);
    const QString secondHighlight = annotations.addHighlight(
        30, 12, QStringLiteral("Another excerpt"), QString(), 0.6, -1);
    QVERIFY(!secondHighlight.isEmpty());

    NotesCenterModel model(state.profileDatabase());
    QCOMPARE(model.totalCount(), 3);
    QCOMPARE(model.bookmarkCount(), 1);
    QCOMPARE(model.highlightCount(), 2);
    QCOMPARE(model.noteCount(), 1);
    QCOMPARE(model.rowCount(), 3);

    model.setTypeFilter(QStringLiteral("bookmarks"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.get(0).value(QStringLiteral("bookTitle")).toString(),
             QStringLiteral("Alpha book"));

    model.setTypeFilter(QStringLiteral("notes"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.get(0).value(QStringLiteral("annotationId")).toString(), firstHighlight);

    model.setTypeFilter(QStringLiteral("all"));
    model.setQuery(QStringLiteral("Bob"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.get(0).value(QStringLiteral("annotationId")).toString(), secondHighlight);

    model.setQuery({});
    QVERIFY(model.updateAnnotation(secondBook,
                                   secondHighlight,
                                   QStringLiteral("Beta label"),
                                   QStringLiteral("Second note")));
    QCOMPARE(model.noteCount(), 2);
    const QVariantMap updated = model.find(secondBook, secondHighlight);
    QCOMPARE(updated.value(QStringLiteral("label")).toString(), QStringLiteral("Beta label"));
    QCOMPARE(updated.value(QStringLiteral("note")).toString(), QStringLiteral("Second note"));

    model.setTypeFilter(QStringLiteral("bookmarks"));
    const QVariantMap bookmark = model.get(0);
    QVERIFY(model.removeAnnotation(bookmark.value(QStringLiteral("sourceUrl")).toUrl(),
                                   bookmark.value(QStringLiteral("annotationId")).toString()));
    QCOMPARE(model.totalCount(), 2);
    QCOMPARE(model.bookmarkCount(), 0);
}

void NotesCenterTest::exportsMarkdownAndHtml()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    LocalStateStore state(directory.filePath(QStringLiteral("settings.ini")));
    const QUrl bookUrl = QUrl::fromLocalFile(directory.filePath(QStringLiteral("notes.txt")));
    state.registerLibraryBook(bookUrl,
                              QStringLiteral("Notes & Essays"),
                              QStringLiteral("Writer"),
                              QStringLiteral("TXT"),
                              {}, {}, {});

    ReadingAnnotationStore annotations(state.profileDatabase());
    annotations.setDocumentUrl(bookUrl);
    QVERIFY(!annotations.addHighlight(4,
                                      8,
                                      QStringLiteral("Quoted <passage>"),
                                      QStringLiteral("Remember <unsafe> & compare"),
                                      0.42,
                                      -1).isEmpty());

    NotesCenterModel model(state.profileDatabase());
    QCOMPARE(model.rowCount(), 1);

    const QString markdownPath = directory.filePath(QStringLiteral("notes.md"));
    QVERIFY(model.exportFiltered(QUrl::fromLocalFile(markdownPath),
                                 QStringLiteral("markdown")));
    QFile markdownFile(markdownPath);
    QVERIFY(markdownFile.open(QIODevice::ReadOnly));
    const QString markdown = QString::fromUtf8(markdownFile.readAll());
    QVERIFY(markdown.contains(QStringLiteral("# SZHBooks reading notes")));
    QVERIFY(markdown.contains(QStringLiteral("Notes & Essays")));
    QVERIFY(markdown.contains(QStringLiteral("Remember <unsafe> & compare")));

    const QString htmlPath = directory.filePath(QStringLiteral("notes.html"));
    QVERIFY(model.exportFiltered(QUrl::fromLocalFile(htmlPath), QStringLiteral("html")));
    QFile htmlFile(htmlPath);
    QVERIFY(htmlFile.open(QIODevice::ReadOnly));
    const QString html = QString::fromUtf8(htmlFile.readAll());
    QVERIFY(html.contains(QStringLiteral("Notes &amp; Essays")));
    QVERIFY(html.contains(QStringLiteral("Quoted &lt;passage&gt;")));
    QVERIFY(html.contains(QStringLiteral("Remember &lt;unsafe&gt; &amp; compare")));
}

QTEST_GUILESS_MAIN(NotesCenterTest)

#include "notescentertest.moc"
