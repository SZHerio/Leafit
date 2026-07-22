import QtQuick
import QtQuick.Controls

CheckBox {
    id: control

    implicitHeight: Theme.compactControlHeight
    spacing: Theme.spaceXs
    activeFocusOnTab: true
    font.family: Theme.uiFontFamily
    font.pixelSize: Theme.bodyFontSize
    Accessible.role: Accessible.CheckBox
    Accessible.name: text

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        color: control.checked ? Theme.accentColor : Theme.surfaceColor
        radius: Theme.radiusSm
        border.color: control.activeFocus
                      ? Theme.focusColor
                      : control.hovered
                        ? Theme.strongBorderColor
                        : Theme.borderColor
        border.width: control.activeFocus ? 2 : 1

        Label {
            anchors.centerIn: parent
            text: "\u2713"
            visible: control.checked
            color: Theme.accentTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: 13
            font.weight: Font.Bold
        }
    }

    contentItem: Label {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? Theme.textColor : Theme.disabledTextColor
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
