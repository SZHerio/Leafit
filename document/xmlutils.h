#pragma once

#include <QDomElement>
#include <QList>
#include <QString>

namespace DocumentXml {

QString elementName(const QDomElement &element);
QDomElement firstElementByName(const QDomNode &root, const QString &name);
QDomElement directChildElementByName(const QDomNode &root, const QString &name);
QList<QDomElement> directChildElementsByName(const QDomNode &root,
                                             const QString &name);
QList<QDomElement> elementsByName(const QDomNode &root, const QString &name);
QString attributeByName(const QDomElement &element, const QString &name);
QString normalizedBlock(QString text);
QString nodeText(const QDomNode &root);

} // namespace DocumentXml
