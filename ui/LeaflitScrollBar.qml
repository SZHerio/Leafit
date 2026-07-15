import QtQuick
import QtQuick.Controls

ScrollBar {
    id: control

    implicitWidth: 12
    implicitHeight: 12
    padding: 4
    policy: ScrollBar.AsNeeded

    contentItem: Rectangle {
        implicitWidth: 4
        implicitHeight: 4
        radius: 2
        color: control.pressed
                   ? Theme.accentColor
                   : control.hovered
                     ? Theme.strongBorderColor
                     : Theme.borderColor
        opacity: control.size < 1.0 ? control.active ? 1.0 : 0.72 : 0.0

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    background: Item {
    }
}
