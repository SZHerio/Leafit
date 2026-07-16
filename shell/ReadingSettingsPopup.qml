import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Popup {
    id: root

    required property var settingsStore
    property bool textSettingsAvailable: true

    readonly property var fontOptions: [
        { value: "serif", label: qsTr("Serif") },
        { value: "sans", label: qsTr("Sans") },
        { value: "mono", label: qsTr("Mono") }
    ]
    readonly property var themeOptions: [
        { value: "light", label: qsTr("Light") },
        { value: "dark", label: qsTr("Dark") }
    ]
    readonly property var alignmentOptions: [
        { value: "left", label: qsTr("Left") },
        { value: "justify", label: qsTr("Justified") }
    ]
    readonly property real hostWindowHeight: root.parent && root.parent.Window.window
                                                   ? root.parent.Window.window.height
                                                   : 720
    readonly property real maximumPanelHeight: Math.max(320,
                                                         hostWindowHeight
                                                         - root.y
                                                         - Theme.spaceMd)

    width: 336
    height: Math.min(settingsColumn.implicitHeight + topPadding + bottomPadding,
                     maximumPanelHeight)
    padding: Theme.spaceLg
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onOpened: settingsFlickable.contentY = 0

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: Flickable {
        id: settingsFlickable

        contentWidth: width
        contentHeight: settingsColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: SZHScrollBar {
        }

        ColumnLayout {
            id: settingsColumn

            width: settingsFlickable.width
                   - (settingsFlickable.contentHeight > settingsFlickable.height
                      ? Theme.spaceSm
                      : 0)
            spacing: Theme.spaceMd

            Label {
                Layout.fillWidth: true
                text: qsTr("Reading settings")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyLargeFontSize
                font.weight: Font.DemiBold
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                Label {
                    text: qsTr("Font")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                SZHSegmentedControl {
                    Layout.fillWidth: true
                    model: root.fontOptions
                    value: root.settingsStore.readingFont
                    onValueSelected: value => root.settingsStore.readingFont = value
                }
            }

            RowLayout {
                visible: root.textSettingsAvailable
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Font size")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                SZHIconButton {
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

                SZHIconButton {
                    symbol: "+"
                    toolTip: qsTr("Increase font size")
                    enabled: root.settingsStore.textFontSize < 36
                    onClicked: root.settingsStore.textFontSize += 1
                }
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                Label {
                    text: qsTr("Alignment")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                SZHSegmentedControl {
                    Layout.fillWidth: true
                    model: root.alignmentOptions
                    value: root.settingsStore.textAlignment
                    onValueSelected: value => root.settingsStore.textAlignment = value
                }
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Paragraph spacing")
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.bodyFontSize
                    }

                    Label {
                        text: root.settingsStore.paragraphSpacing + qsTr(" px")
                        color: Theme.textColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        font.weight: Font.Medium
                    }
                }

                SZHSlider {
                    Layout.fillWidth: true
                    from: 0
                    to: 32
                    stepSize: 2
                    value: root.settingsStore.paragraphSpacing
                    onMoved: root.settingsStore.paragraphSpacing = Math.round(value / 2) * 2
                }
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("First-line indent")
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.bodyFontSize
                    }

                    Label {
                        text: root.settingsStore.firstLineIndent + qsTr(" px")
                        color: Theme.textColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        font.weight: Font.Medium
                    }
                }

                SZHSlider {
                    Layout.fillWidth: true
                    from: 0
                    to: 64
                    stepSize: 4
                    value: root.settingsStore.firstLineIndent
                    onMoved: root.settingsStore.firstLineIndent = Math.round(value / 4) * 4
                }
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
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

                SZHSlider {
                    Layout.fillWidth: true
                    from: 1.2
                    to: 2.0
                    stepSize: 0.05
                    value: root.settingsStore.lineHeight
                    onMoved: root.settingsStore.lineHeight = Math.round(value * 20) / 20
                }
            }

            ColumnLayout {
                visible: root.textSettingsAvailable
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

                SZHSlider {
                    Layout.fillWidth: true
                    from: 560
                    to: 1040
                    stepSize: 20
                    value: root.settingsStore.pageWidth
                    onMoved: root.settingsStore.pageWidth = Math.round(value / 20) * 20
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                RowLayout {
                    Layout.fillWidth: true

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Scroll speed")
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.bodyFontSize
                    }

                    Label {
                        text: qsTr("%1%").arg(root.settingsStore.scrollSpeed)
                        color: Theme.textColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        font.weight: Font.Medium
                    }
                }

                SZHSlider {
                    Layout.fillWidth: true
                    from: 50
                    to: 200
                    stepSize: 10
                    value: root.settingsStore.scrollSpeed
                    onMoved: root.settingsStore.scrollSpeed = Math.round(value / 10) * 10
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceXs

                Label {
                    text: qsTr("Theme")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                }

                SZHSegmentedControl {
                    Layout.fillWidth: true
                    model: root.themeOptions
                    value: root.settingsStore.colorTheme
                    onValueSelected: value => root.settingsStore.colorTheme = value
                }
            }

            SZHButton {
                Layout.fillWidth: true
                text: qsTr("Restore defaults")
                variant: "secondary"
                onClicked: root.settingsStore.resetReadingPreferences()
            }
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
