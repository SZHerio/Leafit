import QtQuick
import QtQuick.Controls

TextArea {
    id: control

    property string accessibleName: placeholderText

    implicitWidth: 280
    implicitHeight: 84
    leftPadding: Theme.spaceSm
    rightPadding: Theme.spaceSm
    topPadding: Theme.spaceSm
    bottomPadding: Theme.spaceSm
    color: Theme.textColor
    placeholderTextColor: Theme.disabledTextColor
    selectionColor: Theme.accentColor
    selectedTextColor: Theme.accentTextColor
    font.family: Theme.uiFontFamily
    font.pixelSize: Theme.bodyFontSize
    wrapMode: TextEdit.Wrap
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.EditableText
    Accessible.name: control.accessibleName
    Accessible.multiLine: true

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
