#include "xmlutils.h"

#include <QDomAttr>
#include <QDomNamedNodeMap>

namespace DocumentXml {

QString elementName(const QDomElement &element)
{
    QString name = element.localName();
    if (name.isEmpty()) {
        name = element.tagName();
    }

    const qsizetype separator = name.indexOf(u':');
    if (separator >= 0) {
        name = name.mid(separator + 1);
    }
    return name;
}

QDomElement firstElementByName(const QDomNode &root, const QString &name)
{
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            return element;
        }

        const QDomElement nested = firstElementByName(node, name);
        if (!nested.isNull()) {
            return nested;
        }
    }
    return {};
}

QDomElement directChildElementByName(const QDomNode &root, const QString &name)
{
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            return element;
        }
    }
    return {};
}

QList<QDomElement> directChildElementsByName(const QDomNode &root,
                                             const QString &name)
{
    QList<QDomElement> elements;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            elements.append(element);
        }
    }
    return elements;
}

QList<QDomElement> elementsByName(const QDomNode &root, const QString &name)
{
    QList<QDomElement> elements;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        const QDomElement element = node.toElement();
        if (!element.isNull() && elementName(element) == name) {
            elements.append(element);
        }
        elements.append(elementsByName(node, name));
    }
    return elements;
}

QString attributeByName(const QDomElement &element, const QString &name)
{
    const QDomNamedNodeMap attributes = element.attributes();
    for (int index = 0; index < attributes.count(); ++index) {
        const QDomAttr attribute = attributes.item(index).toAttr();
        QString attributeName = attribute.localName();
        if (attributeName.isEmpty()) {
            attributeName = attribute.name();
        }

        const qsizetype separator = attributeName.indexOf(u':');
        if (separator >= 0) {
            attributeName = attributeName.mid(separator + 1);
        }
        if (attributeName == name) {
            return attribute.value();
        }
    }
    return {};
}

QString normalizedBlock(QString text)
{
    text.replace(QChar::Nbsp, u' ');
    return text.simplified();
}

QString nodeText(const QDomNode &root)
{
    if (root.isText() || root.isCDATASection()) {
        return root.nodeValue();
    }

    const QDomElement element = root.toElement();
    if (!element.isNull()) {
        const QString name = elementName(element);
        if (name == QLatin1String("br") || name == QLatin1String("empty-line")) {
            return QStringLiteral("\n");
        }
    }

    QString text;
    for (QDomNode node = root.firstChild(); !node.isNull(); node = node.nextSibling()) {
        text += nodeText(node);
    }
    return text;
}

} // namespace DocumentXml
