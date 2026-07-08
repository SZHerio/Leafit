#pragma once

#include "documentloadresult.h"

#include <QUrl>

#include <memory>
#include <vector>

class DocumentReader;

class LocalDocumentLoader
{
public:
    LocalDocumentLoader();
    ~LocalDocumentLoader();

    DocumentLoadResult load(const QUrl &fileUrl) const;

private:
    std::vector<std::unique_ptr<DocumentReader>> m_readers;
};
