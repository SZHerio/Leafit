#pragma once

#include <QString>
#include <QVariantMap>
#include <QtGlobal>

struct ReadingTypography final
{
    static constexpr int minimumFontSize = 12;
    static constexpr int maximumFontSize = 36;
    static constexpr qreal minimumLineHeight = 1.2;
    static constexpr qreal maximumLineHeight = 2.0;
    static constexpr int minimumParagraphSpacing = 0;
    static constexpr int maximumParagraphSpacing = 32;
    static constexpr int minimumFirstLineIndent = 0;
    static constexpr int maximumFirstLineIndent = 64;
    static constexpr int minimumPageWidth = 560;
    static constexpr int maximumPageWidth = 1040;

    QString readingFont = QStringLiteral("serif");
    int fontSize = 18;
    qreal lineHeight = 1.5;
    int paragraphSpacing = 10;
    int firstLineIndent = 24;
    QString textAlignment = QStringLiteral("justify");
    int pageWidth = 820;

    static QString normalizedFont(const QString &value)
    {
        const QString normalized = value.trimmed().toLower();
        return normalized == QLatin1String("sans")
                       || normalized == QLatin1String("mono")
                   ? normalized
                   : QStringLiteral("serif");
    }

    static QString normalizedAlignment(const QString &value)
    {
        return value.trimmed().compare(QStringLiteral("left"),
                                       Qt::CaseInsensitive) == 0
                   ? QStringLiteral("left")
                   : QStringLiteral("justify");
    }

    static ReadingTypography fromVariantMap(const QVariantMap &values)
    {
        ReadingTypography typography;
        typography.readingFont = normalizedFont(
            values.value(QStringLiteral("readingFont"), typography.readingFont).toString());
        typography.fontSize = qBound(
            minimumFontSize,
            values.value(QStringLiteral("fontSize"), typography.fontSize).toInt(),
            maximumFontSize);
        typography.lineHeight = qBound(
            minimumLineHeight,
            values.value(QStringLiteral("lineHeight"), typography.lineHeight).toReal(),
            maximumLineHeight);
        typography.paragraphSpacing = qBound(
            minimumParagraphSpacing,
            values.value(QStringLiteral("paragraphSpacing"),
                         typography.paragraphSpacing).toInt(),
            maximumParagraphSpacing);
        typography.firstLineIndent = qBound(
            minimumFirstLineIndent,
            values.value(QStringLiteral("firstLineIndent"),
                         typography.firstLineIndent).toInt(),
            maximumFirstLineIndent);
        typography.textAlignment = normalizedAlignment(
            values.value(QStringLiteral("textAlignment"),
                         typography.textAlignment).toString());
        typography.pageWidth = qBound(
            minimumPageWidth,
            values.value(QStringLiteral("pageWidth"), typography.pageWidth).toInt(),
            maximumPageWidth);
        return typography;
    }

    QVariantMap toVariantMap(bool enabled = true) const
    {
        return {
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("readingFont"), readingFont},
            {QStringLiteral("fontSize"), fontSize},
            {QStringLiteral("lineHeight"), lineHeight},
            {QStringLiteral("paragraphSpacing"), paragraphSpacing},
            {QStringLiteral("firstLineIndent"), firstLineIndent},
            {QStringLiteral("textAlignment"), textAlignment},
            {QStringLiteral("pageWidth"), pageWidth}
        };
    }
};
