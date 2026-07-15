import QtQuick
import QtQuick.Controls

ProgressBar {
    id: control

    implicitWidth: 220
    implicitHeight: 4

    background: Rectangle {
        implicitHeight: 4
        color: Theme.surfaceMutedColor
        radius: height / 2
    }

    contentItem: Item {
        implicitHeight: 4
        clip: true

        Rectangle {
            width: parent.width * control.visualPosition
            height: parent.height
            color: Theme.accentColor
            radius: height / 2

            Behavior on width {
                NumberAnimation {
                    duration: Theme.motionFast
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
}
