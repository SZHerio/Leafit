import QtQuick
import QtQuick.Controls

Slider {
    id: control

    implicitWidth: 240
    implicitHeight: 28
    leftPadding: 0
    rightPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    background: Rectangle {
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)
        width: control.availableWidth
        height: 4
        radius: height / 2
        color: Theme.surfaceMutedColor

        Rectangle {
            width: parent.width * control.visualPosition
            height: parent.height
            radius: parent.radius
            color: Theme.accentColor
        }
    }

    handle: Rectangle {
        x: control.leftPadding
           + control.visualPosition * (control.availableWidth - width)
        y: Math.round((control.height - height) / 2)
        width: 16
        height: 16
        radius: width / 2
        color: control.pressed ? Theme.accentPressedColor : Theme.accentColor
        border.color: control.activeFocus ? Theme.focusColor : Theme.surfaceColor
        border.width: 2

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
}
