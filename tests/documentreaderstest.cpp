#include "../documentreaders.h"

#include <QFile>
#include <QTemporaryDir>
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

} // namespace

class DocumentReadersTest final : public QObject
{
    Q_OBJECT

private slots:
    void extractsFb2MetadataAndChapters();
    void extractsEpub3NavigationInSpineOrder();
    void extractsEpub2NcxNavigation();
    void readsDocxThroughStableArchiveLayer();
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
      <author><nickname>Leaflit Author</nickname></author>
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
    QCOMPARE(result.author, QStringLiteral("Ada Lovelace, Leaflit Author"));
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
    QCOMPARE(result.text, QStringLiteral("First paragraph.\n\nSecond paragraph."));
}

QTEST_GUILESS_MAIN(DocumentReadersTest)

#include "documentreaderstest.moc"
