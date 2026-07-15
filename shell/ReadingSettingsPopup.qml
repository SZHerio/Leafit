import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    required property var settingsStore

    width: 304
    padding: Theme.spaceLg
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.spaceMd

        Label {
            text: qsTr("Reading settings")
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyLargeFontSize
            font.weight: Font.DemiBold
            Layout.fillWidth: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Label {
                Layout.fillWidth: true
                text: qsTr("Font size")
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
            }

            LeaflitIconButton {
                symbol: "-"
                toolTip: qsTr("Decrease font size")
                enabled: root.settingsStore.textFontSize > 12
                onClicked: root.settingsStore.textFontSize -= 1
            }

            Label {
                Layout.preferredWidth: 34
                text: root.settingsStore.textFontSize
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            LeaflitIconButton {
                symbol: "+"
                toolTip: qsTr("Increase font size")
                enabled: root.settingsStore.textFontSize < 36
                onClicked: root.settingsStore.textFontSize += 1
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Line spacing")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                Label {
                    text: Number(root.settingsStore.lineHeight).toFixed(2)
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                    font.weight: Font.Medium
                }
            }

            LeaflitSlider {
                Layout.fillWidth: true
                from: 1.2
                to: 2.0
                stepSize: 0.05
                value: root.settingsStore.lineHeight
                onMoved: root.settingsStore.lineHeight = Math.round(value * 20) / 20
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Text width")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                Label {
                    text: root.settingsStore.pageWidth + qsTr(" px")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                    font.weight: Font.Medium
                }
            }

            LeaflitSlider {
                Layout.fillWidth: true
                from: 560
                to: 1040
                stepSize: 20
                value: root.settingsStore.pageWidth
                onMoved: root.settingsStore.pageWidth = Math.round(value / 20) * 20
            }
        }

        LeaflitSwitch {
            Layout.fillWidth: true
            text: qsTr("Dark theme")
            checked: root.settingsStore.darkMode
            onToggled: root.settingsStore.darkMode = checked
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: Theme.motionFast
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: Theme.motionFast
        }
    }
}
