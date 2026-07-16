#include "../documentreaders.h"

#include <QFile>
#include <QPdfDocument>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextImageFormat>
#include <QtTest>

#include <miniz.h>

namespace {

struct ArchiveEntry final
{
    QByteArray path;
    QByteArray data;
};

bool writeArchive(const QString &filePath, const QList<ArchiveEntry> &entries)
{
    mz_zip_archive archive{};
    if (!mz_zip_writer_init_heap(&archive, 0, 0)) {
        return false;
    }

    bool success = true;
    for (const ArchiveEntry &entry : entries) {
        if (!mz_zip_writer_add_mem(&archive,
                                   entry.path.constData(),
                                   entry.data.constData(),
                                   static_cast<size_t>(entry.data.size()),
                                   MZ_BEST_SPEED)) {
            success = false;
            break;
        }
    }

    void *archiveData = nullptr;
    size_t archiveSize = 0;
    if (success) {
        success = mz_zip_writer_finalize_heap_archive(&archive,
                                                      &archiveData,
                                                      &archiveSize);
    }

    if (success) {
        QFile file(filePath);
        success = file.open(QIODevice::WriteOnly)
                  && file.write(static_cast<const char *>(archiveData),
                                static_cast<qint64>(archiveSize))
                         == static_cast<qint64>(archiveSize);
    }

    mz_free(archiveData);
    mz_zip_writer_end(&archive);
    return success;
}

QByteArray containerXml()
{
    return R"XML(<?xml version="1.0"?>
<container xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles><rootfile full-path="OEBPS/content.opf"/></rootfiles>
</container>)XML";
}

QByteArray tinyPng()
{
    return QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
}

QByteArray encryptedPdf()
{
    return QByteArray::fromBase64(
        "JVBERi0xLjMKJeLjz9MKMSAwIG9iago8PAovUHJvZHVjZXIgPGE2NjUzYmRhZDI+Cj4+CmVuZG9iagoyIDAgb2JqCjw8Ci9U"
        "eXBlIC9QYWdlcwovQ291bnQgMQovS2lkcyBbIDQgMCBSIF0KPj4KZW5kb2JqCjMgMCBvYmoKPDwKL1R5cGUgL0NhdGFsb2cK"
        "L1BhZ2VzIDIgMCBSCj4+CmVuZG9iago0IDAgb2JqCjw8Ci9UeXBlIC9QYWdlCi9SZXNvdXJjZXMgPDwKPj4KL01lZGlhQm94"
        "IFsgMC4wIDAuMCA3MiA3MiBdCi9QYXJlbnQgMiAwIFIKPj4KZW5kb2JqCjUgMCBvYmoKPDwKL1YgMgovUiAzCi9MZW5ndGgg"
        "MTI4Ci9QIDQyOTQ5NjcyOTIKL0ZpbHRlciAvU3RhbmRhcmQKL08gPDM4NTBkMjU0OGNiZWUyMjFjZWNiOWNkM2UzYTcyN2Q0"
        "YzllM2NkN2I0MzliMDM5YzI2YWUyZmIwYTY3ODMwNTc+Ci9VIDw0YmMwZTEyZjg4N2JiODk2MTg3OTQxZTVmZWJjOGUxOTI4"
        "YmY0ZTVlNGU3NThhNDE2NDAwNGU1NmZmZmEwMTA4Pgo+PgplbmRvYmoKeHJlZgowIDYKMDAwMDAwMDAwMCA2NTUzNSBmIAow"
        "MDAwMDAwMDE1IDAwMDAwIG4gCjAwMDAwMDAwNTkgMDAwMDAgbiAKMDAwMDAwMDExOCAwMDAwMCBuIAowMDAwMDAwMTY3IDAw"
        "MDAwIG4gCjAwMDAwMDAyNTkgMDAwMDAgbiAKdHJhaWxlcgo8PAovU2l6ZSA2Ci9Sb290IDMgMCBSCi9JbmZvIDEgMCBSCi9J"
        "RCBbIDw2NDY2NjM2MTY2MzUzNDMyMzczOTMwMzMzMjMwMzY2NDY0MzEzMTM0MzQzMTY0MzA2MjM1NjI2MTM5MzYzMTYyPiA8"
        "NjQ2NjYzNjE2NjM1MzQzMjM3MzkzMDMzMzIzMDM2NjQ2NDMxMzEzNDM0MzE2NDMwNjIzNTYyNjEzOTM2MzE2Mj4gXQovRW5j"
        "cnlwdCA1IDAgUgo+PgpzdGFydHhyZWYKNDc0CiUlRU9GCg==");
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
           && file.write(data) == data.size();
}

bool richTextHasImage(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()
                && fragment.charFormat().isImageFormat()
                && fragment.charFormat().toImageFormat().name().startsWith(
                    QStringLiteral("data:image/"))) {
                return true;
            }
        }
    }
    return false;
}

bool richTextImagesAreIsolated(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        bool hasImage = false;
        bool hasText = false;
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            hasImage = hasImage || fragment.charFormat().isImageFormat();
            hasText = hasText || (!fragment.charFormat().isImageFormat()
                                  && !fragment.text().trimmed().isEmpty());
        }
        if (hasImage && hasText) {
            return false;
        }
    }
    return true;
}

bool richTextHasLink(const QString &html, const QString &href)
{
    QTextDocument document;
    document.setHtml(html);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()
                && fragment.charFormat().anchorHref() == href) {
                return true;
            }
        }
    }
    return false;
}

bool richTextHasAnchor(const QString &html, const QString &anchor)
{
    QTextDocument document;
    document.setHtml(html);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()
                && fragment.charFormat().anchorNames().contains(anchor)) {
                return true;
            }
        }
    }
    return false;
}

bool richTextHasItalicText(const QString &html, const QString &text)
{
    QTextDocument document;
    document.setHtml(html);
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::Iterator iterator = block.begin();
             !iterator.atEnd();
             ++iterator) {
            const QTextFragment fragment = iterator.fragment();
            if (fragment.isValid()
                && fragment.text().contains(text)
                && fragment.charFormat().fontItalic()) {
                return true;
            }
        }
    }
    return false;
}

bool richTextHasDocumentStructures(const QString &html)
{
    QTextDocument document;
    document.setHtml(html);
    bool heading = false;
    bool list = false;
    bool table = false;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        heading = heading || block.blockFormat().headingLevel() > 0;
        list = list || block.textList();
        QTextCursor cursor(block);
        table = table || cursor.currentTable();
    }
    return heading && list && table;
}

} // namespace

class DocumentReadersTest final : public QObject
{
    Q_OBJECT

private slots:
    void extractsFb2MetadataAndChapters();
    void extractsEpub3NavigationInSpineOrder();
    void extractsEpub2NcxNavigation();
    void readsDocxThroughStableArchiveLayer();
    void rendersHtmlAndMarkdownResources();
    void rendersFb2IllustrationsAndNotes();
    void rendersEpubStylesImagesAndFootnotes();
    void rendersDocxStructuresAndImages();
    void acceptsProtectedPdfForInteractiveUnlock();
};

void DocumentReadersTest::extractsFb2MetadataAndChapters()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString filePath = directory.filePath(QStringLiteral("sample.fb2"));
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(R"FB2(<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description>
    <title-info>
      <book-title>Sample Book</book-title>
      <author><first-name>Ada</first-name><last-name>Lovelace</last-name></author>
      <author><nickname>SZHBooks Author</nickname></author>
    </title-info>
  </description>
  <body>
    <section><title><p>Chapter One</p></title><p>First chapter text.</p></section>
    <section><title><p>Chapter Two</p></title><p>Second chapter text with more words.</p></section>
  </body>
</FictionBook>)FB2");
    file.close();

    const DocumentLoadResult result = Fb2DocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.title, QStringLiteral("Sample Book"));
    QCOMPARE(result.author, QStringLiteral("Ada Lovelace, SZHBooks Author"));
    QCOMPARE(result.chapters.size(), 2);
    QCOMPARE(result.chapters.at(0).title, QStringLiteral("Chapter One"));
    QCOMPARE(result.chapters.at(0).progress, 0.0);
    QCOMPARE(result.chapters.at(1).title, QStringLiteral("Chapter Two"));
    QVERIFY(result.chapters.at(1).progress > result.chapters.at(0).progress);
}

void DocumentReadersTest::extractsEpub3NavigationInSpineOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString filePath = directory.filePath(QStringLiteral("sample-epub3.epub"));
    QVERIFY(writeArchive(
        filePath,
        {
            {"META-INF/container.xml", containerXml()},
            {"OEBPS/content.opf",
             R"OPF(<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>EPUB 3 Sample</dc:title><dc:creator>Ursula Reader</dc:creator>
  </metadata>
  <manifest>
    <item id="one" href="one.xhtml" media-type="application/xhtml+xml"/>
    <item id="two" href="two.xhtml" media-type="application/xhtml+xml"/>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
  </manifest>
  <spine><itemref idref="two"/><itemref idref="one"/></spine>
</package>)OPF"},
            {"OEBPS/nav.xhtml",
             R"HTML(<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<body><nav epub:type="toc"><ol>
  <li><a href="two.xhtml#part-start">Second act</a></li>
  <li><a href="two.xhtml#landing">The arrival</a></li>
  <li><a href="one.xhtml#opening">A quiet opening</a></li>
</ol></nav></body></html>)HTML"},
            {"OEBPS/one.xhtml",
             R"HTML(<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Fallback One</title></head>
<body><h1 id="opening">Opening</h1><p>Opening chapter text.</p></body></html>)HTML"},
            {"OEBPS/two.xhtml",
             R"HTML(<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Fallback Two</title></head>
<body><h1 id="part-start">Part Two</h1><p>Preamble before arrival.</p>
<h2 id="landing">Arrival</h2><p>Arrival chapter text with additional words.</p></body></html>)HTML"}
        }));

    const DocumentLoadResult result = EpubDocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.title, QStringLiteral("EPUB 3 Sample"));
    QCOMPARE(result.author, QStringLiteral("Ursula Reader"));
    QCOMPARE(result.chapters.size(), 3);
    QCOMPARE(result.chapters.at(0).title, QStringLiteral("Second act"));
    QCOMPARE(result.chapters.at(0).progress, 0.0);
    QCOMPARE(result.chapters.at(1).title, QStringLiteral("The arrival"));
    QVERIFY(result.chapters.at(1).progress > result.chapters.at(0).progress);
    QCOMPARE(result.chapters.at(2).title, QStringLiteral("A quiet opening"));
    QVERIFY(result.chapters.at(2).progress > result.chapters.at(1).progress);
    QVERIFY(result.text.indexOf(QStringLiteral("Part Two"))
            < result.text.indexOf(QStringLiteral("Opening")));
}

void DocumentReadersTest::extractsEpub2NcxNavigation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString filePath = directory.filePath(QStringLiteral("sample-epub2.epub"));
    QVERIFY(writeArchive(
        filePath,
        {
            {"META-INF/container.xml", containerXml()},
            {"OEBPS/content.opf",
             R"OPF(<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>EPUB 2 Sample</dc:title><dc:creator>Octavia Example</dc:creator>
  </metadata>
  <manifest>
    <item id="chapter-a" href="a.xhtml" media-type="application/xhtml+xml"/>
    <item id="chapter-b" href="b.xhtml" media-type="application/xhtml+xml"/>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx"><itemref idref="chapter-a"/><itemref idref="chapter-b"/></spine>
</package>)OPF"},
            {"OEBPS/toc.ncx",
             R"NCX(<?xml version="1.0"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/">
  <navMap>
    <navPoint><navLabel><text>NCX beginning</text></navLabel><content src="a.xhtml"/></navPoint>
    <navPoint><navLabel><text>NCX ending</text></navLabel><content src="b.xhtml"/></navPoint>
  </navMap>
</ncx>)NCX"},
            {"OEBPS/a.xhtml", "<html><body><h1>Alpha</h1><p>First text.</p></body></html>"},
            {"OEBPS/b.xhtml", "<html><body><h1>Beta</h1><p>Second text.</p></body></html>"}
        }));

    const DocumentLoadResult result = EpubDocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.title, QStringLiteral("EPUB 2 Sample"));
    QCOMPARE(result.author, QStringLiteral("Octavia Example"));
    QCOMPARE(result.chapters.size(), 2);
    QCOMPARE(result.chapters.at(0).title, QStringLiteral("NCX beginning"));
    QCOMPARE(result.chapters.at(1).title, QStringLiteral("NCX ending"));
    QVERIFY(result.chapters.at(1).progress > result.chapters.at(0).progress);
}

void DocumentReadersTest::readsDocxThroughStableArchiveLayer()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString filePath = directory.filePath(
        QStringLiteral("unicode-\u043a\u043d\u0438\u0433\u0430.docx"));
    QVERIFY(writeArchive(
        filePath,
        {{"word/document.xml",
          R"DOCX(<?xml version="1.0"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:body><w:p><w:r><w:t>First paragraph.</w:t></w:r></w:p>
  <w:p><w:r><w:t>Second paragraph.</w:t></w:r></w:p></w:body>
</w:document>)DOCX"}}));

    const DocumentLoadResult result = DocxDocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QCOMPARE(result.formatName, QStringLiteral("DOCX"));
    QCOMPARE(result.text, QStringLiteral("First paragraph.\nSecond paragraph."));
    QVERIFY(result.richText);
}

void DocumentReadersTest::rendersHtmlAndMarkdownResources()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeFile(directory.filePath(QStringLiteral("pixel.png")), tinyPng()));

    const QString htmlPath = directory.filePath(QStringLiteral("sample.html"));
    QVERIFY(writeFile(
        htmlPath,
        R"HTML(<html><head><title>Local HTML</title></head><body>
<h1>HTML chapter</h1><p><a href="https://qt.io">Qt website</a></p>
<img src="pixel.png" alt="Pixel"></body></html>)HTML"));
    const DocumentLoadResult htmlResult = HtmlDocumentReader().load(QFileInfo(htmlPath));
    QVERIFY(htmlResult.isSuccess());
    QVERIFY(htmlResult.richText);
    QCOMPARE(htmlResult.title, QStringLiteral("Local HTML"));
    QVERIFY(richTextHasImage(htmlResult.displayText));
    QVERIFY(richTextImagesAreIsolated(htmlResult.displayText));
    QVERIFY(richTextHasLink(htmlResult.displayText, QStringLiteral("https://qt.io")));

    const QString markdownPath = directory.filePath(QStringLiteral("sample.md"));
    QVERIFY(writeFile(
        markdownPath,
        QByteArray("# Markdown chapter\n\n[Qt website](https://qt.io)\n\n![Pixel](pixel.png)")));
    const DocumentLoadResult markdownResult = MarkdownDocumentReader().load(
        QFileInfo(markdownPath));
    QVERIFY(markdownResult.isSuccess());
    QVERIFY(markdownResult.richText);
    QVERIFY(richTextHasImage(markdownResult.displayText));
    QVERIFY(richTextHasLink(markdownResult.displayText,
                            QStringLiteral("https://qt.io")));
}

void DocumentReadersTest::rendersFb2IllustrationsAndNotes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("rich.fb2"));
    const QString fb2 = QString::fromLatin1(R"FB2(<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0"
             xmlns:l="http://www.w3.org/1999/xlink">
  <description><title-info><book-title>Illustrated FB2</book-title></title-info></description>
  <body><section><title><p>Main chapter</p></title>
    <p>Reference <a l:href="#note-1" type="note">1</a></p>
    <image l:href="#illustration" alt="Illustration"/>
  </section></body>
  <body name="notes"><section id="note-1"><title><p>Footnote</p></title>
    <p>Footnote text.</p></section></body>
  <binary id="illustration" content-type="image/png">%1</binary>
</FictionBook>)FB2")
                            .arg(QString::fromLatin1(tinyPng().toBase64()));
    QVERIFY(writeFile(filePath, fb2.toUtf8()));

    const DocumentLoadResult result = Fb2DocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QVERIFY(result.richText);
    QVERIFY(richTextHasImage(result.displayText));
    QVERIFY(richTextImagesAreIsolated(result.displayText));
    QVERIFY(richTextHasLink(result.displayText, QStringLiteral("#note-1")));
    QVERIFY(richTextHasAnchor(result.displayText, QStringLiteral("note-1")));
    QVERIFY(result.text.contains(QStringLiteral("Footnote text.")));
    QCOMPARE(result.chapters.size(), 1);
}

void DocumentReadersTest::rendersEpubStylesImagesAndFootnotes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("rich.epub"));
    QVERIFY(writeArchive(
        filePath,
        {
            {"META-INF/container.xml", containerXml()},
            {"OEBPS/content.opf",
             R"OPF(<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Rich EPUB</dc:title><dc:creator>Format Reader</dc:creator>
  </metadata>
  <manifest>
    <item id="chapter" href="chapter.xhtml" media-type="application/xhtml+xml"/>
    <item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>
    <item id="css" href="styles/book.css" media-type="text/css"/>
    <item id="image" href="images/pixel.png" media-type="image/png"/>
  </manifest>
  <spine><itemref idref="chapter"/><itemref idref="notes" linear="no"/></spine>
</package>)OPF"},
            {"OEBPS/chapter.xhtml",
             R"HTML(<html xmlns="http://www.w3.org/1999/xhtml"><head>
<title>Chapter</title><link rel="stylesheet" href="styles/book.css"/></head><body>
<h1 id="start">Chapter</h1><p class="accent">Styled paragraph.</p>
<p><a href="notes.xhtml#note-1">Open footnote</a></p>
<img src="images/pixel.png" alt="Pixel"/></body></html>)HTML"},
            {"OEBPS/notes.xhtml",
             R"HTML(<html xmlns="http://www.w3.org/1999/xhtml"><body>
<h1>Notes</h1><p id="note-1">Footnote text.</p>
<a href="chapter.xhtml#start">Back</a></body></html>)HTML"},
            {"OEBPS/styles/book.css", ".accent { font-style: italic; }"},
            {"OEBPS/images/pixel.png", tinyPng()}
        }));

    const DocumentLoadResult result = EpubDocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QVERIFY(result.richText);
    QCOMPARE(result.title, QStringLiteral("Rich EPUB"));
    QVERIFY(richTextHasImage(result.displayText));
    QVERIFY(richTextImagesAreIsolated(result.displayText));
    QVERIFY(richTextHasItalicText(result.displayText,
                                  QStringLiteral("Styled paragraph.")));
    QVERIFY(richTextHasLink(result.displayText,
                            QStringLiteral("#epub-1-note-1")));
    QVERIFY(richTextHasAnchor(result.displayText,
                              QStringLiteral("epub-1-note-1")));
    QVERIFY(result.text.contains(QStringLiteral("Footnote text.")));
}

void DocumentReadersTest::rendersDocxStructuresAndImages()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString filePath = directory.filePath(QStringLiteral("rich.docx"));
    QVERIFY(writeArchive(
        filePath,
        {
            {"word/document.xml",
             R"DOCX(<?xml version="1.0"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
 xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
 xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
 xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing">
<w:body>
  <w:p><w:pPr><w:pStyle w:val="Heading1"/></w:pPr><w:r><w:t>Rich chapter</w:t></w:r></w:p>
  <w:p><w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/></w:numPr></w:pPr>
    <w:r><w:t>First list item</w:t></w:r></w:p>
  <w:p><w:hyperlink r:id="rIdLink"><w:r><w:rPr><w:b/><w:i/></w:rPr>
    <w:t>Qt website</w:t></w:r></w:hyperlink></w:p>
  <w:p><w:r><w:drawing><wp:inline><wp:extent cx="914400" cy="914400"/>
    <a:graphic><a:graphicData><a:blip r:embed="rIdImage"/></a:graphicData></a:graphic>
  </wp:inline></w:drawing></w:r></w:p>
  <w:tbl><w:tr><w:tc><w:p><w:r><w:t>Cell A</w:t></w:r></w:p></w:tc>
    <w:tc><w:p><w:r><w:t>Cell B</w:t></w:r></w:p></w:tc></w:tr></w:tbl>
</w:body></w:document>)DOCX"},
            {"word/styles.xml",
             R"XML(<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:style w:type="paragraph" w:styleId="Heading1"><w:name w:val="heading 1"/></w:style>
</w:styles>)XML"},
            {"word/numbering.xml",
             R"XML(<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:abstractNum w:abstractNumId="1"><w:lvl w:ilvl="0"><w:numFmt w:val="bullet"/></w:lvl></w:abstractNum>
<w:num w:numId="1"><w:abstractNumId w:val="1"/></w:num></w:numbering>)XML"},
            {"word/_rels/document.xml.rels",
             R"XML(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rIdLink" Type="hyperlink" Target="https://qt.io" TargetMode="External"/>
<Relationship Id="rIdImage" Type="image" Target="media/pixel.png"/>
</Relationships>)XML"},
            {"word/media/pixel.png", tinyPng()},
            {"docProps/core.xml",
             R"XML(<cp:coreProperties xmlns:cp="http://schemas.openxmlformats.org/package/2006/metadata/core-properties"
 xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>Rich DOCX</dc:title>
<dc:creator>Grace Hopper</dc:creator></cp:coreProperties>)XML"}
        }));

    const DocumentLoadResult result = DocxDocumentReader().load(QFileInfo(filePath));
    QVERIFY(result.isSuccess());
    QVERIFY(result.richText);
    QCOMPARE(result.title, QStringLiteral("Rich DOCX"));
    QCOMPARE(result.author, QStringLiteral("Grace Hopper"));
    QCOMPARE(result.chapters.size(), 1);
    QCOMPARE(result.chapters.first().title, QStringLiteral("Rich chapter"));
    QVERIFY(richTextHasImage(result.displayText));
    QVERIFY(richTextImagesAreIsolated(result.displayText));
    QVERIFY(richTextHasLink(result.displayText, QStringLiteral("https://qt.io")));
    QVERIFY(richTextHasItalicText(result.displayText, QStringLiteral("Qt website")));
    QVERIFY(richTextHasDocumentStructures(result.displayText));
}

void DocumentReadersTest::acceptsProtectedPdfForInteractiveUnlock()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString protectedPath = directory.filePath(QStringLiteral("protected.pdf"));
    QVERIFY(writeFile(protectedPath, encryptedPdf()));
    const DocumentLoadResult protectedResult = PdfDocumentReader().load(
        QFileInfo(protectedPath));
    QVERIFY(protectedResult.isSuccess());
    QCOMPARE(protectedResult.viewMode, DocumentViewMode::Pdf);
    QCOMPARE(protectedResult.title, QStringLiteral("protected"));

    QPdfDocument unlockedDocument;
    unlockedDocument.setPassword(QStringLiteral("szhbooks-test"));
    QCOMPARE(unlockedDocument.load(protectedPath), QPdfDocument::Error::None);
    QCOMPARE(unlockedDocument.pageCount(), 1);

    const QString invalidPath = directory.filePath(QStringLiteral("invalid.pdf"));
    QVERIFY(writeFile(invalidPath, QByteArray("not a PDF")));
    const DocumentLoadResult invalidResult = PdfDocumentReader().load(
        QFileInfo(invalidPath));
    QVERIFY(!invalidResult.isSuccess());
    QVERIFY(!invalidResult.errorMessage.isEmpty());
}

QTEST_MAIN(DocumentReadersTest)

#include "documentreaderstest.moc"
