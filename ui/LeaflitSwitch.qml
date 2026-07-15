import QtQuick
import QtQuick.Controls

Switch {
    id: control

    property bool onChrome: false

    implicitWidth: Math.max(82, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spaceXs
    rightPadding: indicator.width + Theme.spaceSm
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    font.family: Theme.uiFontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Font.Medium

    contentItem: Text {
        text: control.text
        color: control.onChrome ? Theme.chromeTextColor : Theme.textColor
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Rectangle {
        implicitWidth: 38
        implicitHeight: 22
        x: control.width - width
        y: Math.round((control.height - height) / 2)
        radius: height / 2
        color: control.checked
                   ? Theme.accentColor
                   : control.onChrome
                     ? Theme.chromeHoverColor
                     : Theme.surfaceMutedColor
        border.color: control.checked
                          ? Theme.accentColor
                          : control.onChrome
                            ? Theme.chromeMutedTextColor
                            : Theme.strongBorderColor

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionNormal
            }
        }

        Rectangle {
            width: 16
            height: 16
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            radius: width / 2
            color: control.checked ? Theme.onAccentColor : Theme.surfaceColor

            Behavior on x {
                NumberAnimation {
                    duration: Theme.motionNormal
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    background: Rectangle {
        color: "transparent"
        radius: Theme.radiusMd
        border.color: control.activeFocus ? Theme.focusColor : "transparent"
        border.width: control.activeFocus ? 2 : 0
    }
}
