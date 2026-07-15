import QtQuick
import QtQuick.Controls

TextField {
    id: control

    implicitWidth: 280
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spaceSm
    rightPadding: Theme.spaceSm
    topPadding: 0
    bottomPadding: 0
    color: Theme.textColor
    placeholderTextColor: Theme.disabledTextColor
    selectionColor: Theme.accentColor
    selectedTextColor: Theme.onAccentColor
    font.family: Theme.uiFontFamily
    font.pixelSize: Theme.bodyFontSize
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: control.activeFocus
                          ? Theme.focusColor
                          : control.hovered
                            ? Theme.strongBorderColor
                            : Theme.borderColor
        border.width: control.activeFocus ? 2 : 1

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
}
