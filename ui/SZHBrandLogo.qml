import QtQuick

Item {
    id: root

    property bool iconOnly: false
    property bool darkVariant: Theme.darkMode

    implicitWidth: iconOnly ? 48 : 176
    implicitHeight: iconOnly ? 48 : 44

    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("SZHBooks")

    Image {
        anchors.fill: parent
        source: "../assets/branding/szhbooks-logo-sheet-transparent.png"
        sourceClipRect: root.iconOnly
                        ? Qt.rect(127, root.darkVariant ? 595 : 102, 389, 355)
                        : Qt.rect(125, root.darkVariant ? 593 : 100, 1295, 359)
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
    }
}
