#pragma once

#include "documentreader.h"

class PlainTextDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class HtmlDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class MarkdownDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class Fb2DocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class EpubDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class DocxDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};

class PdfDocumentReader final : public DocumentReader
{
public:
    QStringList suffixes() const override;
    DocumentLoadResult load(const QFileInfo &fileInfo) const override;
};
