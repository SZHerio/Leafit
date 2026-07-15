import QtQuick

Rectangle {
    property color fillColor: Theme.surfaceColor
    property bool outlined: true

    color: fillColor
    radius: Theme.radiusLg
    border.color: outlined ? Theme.borderColor : "transparent"
    border.width: outlined ? 1 : 0

    Behavior on color {
        ColorAnimation {
            duration: Theme.motionNormal
        }
    }
}
