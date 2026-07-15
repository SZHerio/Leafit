import QtQuick
import QtQuick.Controls

Button {
    id: control

    property string variant: "primary"
    property string symbol: ""

    readonly property color resolvedForegroundColor: {
        if (!enabled) {
            return variant === "chrome" ? Theme.chromeMutedTextColor : Theme.disabledTextColor
        }
        if (variant === "primary") {
            return Theme.primaryActionTextColor
        }
        if (variant === "chrome") {
            return Theme.chromeTextColor
        }
        return Theme.textColor
    }

    readonly property color resolvedBackgroundColor: {
        if (variant === "primary") {
            if (!enabled) {
                return Theme.accentSoftColor
            }
            return down ? Theme.primaryActionPressedColor
                        : hovered ? Theme.primaryActionHoverColor
                                  : Theme.primaryActionColor
        }
        if (variant === "chrome") {
            return down ? Theme.chromePressedColor
                        : hovered ? Theme.chromeHoverColor
                                  : "transparent"
        }
        if (variant === "secondary") {
            return down ? Theme.accentSoftColor
                        : hovered ? Theme.surfaceMutedColor
                                  : Theme.surfaceColor
        }
        return down ? Theme.accentSoftColor
                    : hovered ? Theme.surfaceMutedColor
                              : "transparent"
    }

    readonly property color resolvedBorderColor: activeFocus
                                                    ? Theme.focusColor
                                                    : variant === "secondary"
                                                      ? Theme.borderColor
                                                      : "transparent"

    implicitWidth: Math.max(80, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlHeight
    leftPadding: Theme.spaceMd
    rightPadding: Theme.spaceMd
    topPadding: 0
    bottomPadding: 0
    spacing: Theme.spaceXs
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    palette.buttonText: resolvedForegroundColor

    font.family: Theme.uiFontFamily
    font.pixelSize: Theme.bodyFontSize
    font.weight: Font.DemiBold

    contentItem: Row {
        spacing: control.spacing

        Text {
            visible: control.symbol.length > 0
            anchors.verticalCenter: parent.verticalCenter
            text: control.symbol
            color: control.resolvedForegroundColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.iconFontSize
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            color: control.resolvedForegroundColor
            font: control.font
        }
    }

    background: Rectangle {
        color: control.resolvedBackgroundColor
        radius: Theme.radiusMd
        border.color: control.resolvedBorderColor
        border.width: control.activeFocus ? 2 : control.variant === "secondary" ? 1 : 0

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }
}
