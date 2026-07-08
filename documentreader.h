#pragma once

#include "documentloadresult.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>

class DocumentReader
{
public:
    virtual ~DocumentReader() = default;

    bool supportsSuffix(const QString &suffix) const
    {
        return suffixes().contains(suffix.toLower());
    }

    virtual QStringList suffixes() const = 0;
    virtual DocumentLoadResult load(const QFileInfo &fileInfo) const = 0;
};
