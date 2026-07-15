import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var readerController
    required property var readerWorkspace
    required property var settingsStore
    property bool darkMode: false

    signal openRequested
    signal darkModeToggled(bool darkMode)

    implicitHeight: Theme.toolbarHeight
    color: Theme.chromeColor

    Behavior on color {
        ColorAnimation {
            duration: Theme.motionNormal
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceLg
        anchors.rightMargin: Theme.spaceLg
        spacing: Theme.spaceSm

        Label {
            text: qsTr("Leaflit")
            color: Theme.chromeTextColor
            font.family: Theme.readingFontFamily
            font.pixelSize: Theme.titleFontSize
            font.weight: Font.DemiBold
            Layout.rightMargin: Theme.spaceSm
        }

        LeaflitButton {
            text: qsTr("Open book")
            variant: "chrome"
            onClicked: root.openRequested()
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceSm
            Layout.rightMargin: Theme.spaceSm
            text: root.readerController.title
            color: Theme.chromeMutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            visible: root.readerWorkspace.hasDocument
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            Layout.leftMargin: Theme.spaceXs
            Layout.rightMargin: Theme.spaceXs
            color: Theme.chromeBorderColor
        }

        LeaflitIconButton {
            visible: root.readerWorkspace.hasDocument
            symbol: "-"
            toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom out")
                         : qsTr("Decrease font size")
            onChrome: true
            enabled: root.readerWorkspace.canDecreaseScale
            onClicked: root.readerWorkspace.decreaseScale()
        }

        Label {
            visible: root.readerWorkspace.hasDocument
            text: root.readerWorkspace.scaleLabel
            color: Theme.chromeMutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.preferredWidth: 58
        }

        LeaflitIconButton {
            visible: root.readerWorkspace.hasDocument
            symbol: "+"
            toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom in")
                         : qsTr("Increase font size")
            onChrome: true
            enabled: root.readerWorkspace.canIncreaseScale
            onClicked: root.readerWorkspace.increaseScale()
        }

        LeaflitIconButton {
            visible: root.readerWorkspace.showingPdf
            symbol: "\u2922"
            toolTip: qsTr("Fit page to width")
            onChrome: true
            onClicked: root.readerWorkspace.fitPdfToWidth()
        }

        LeaflitIconButton {
            symbol: "Aa"
            symbolPixelSize: Theme.bodyFontSize
            toolTip: qsTr("Reading settings")
            onChrome: true
            selected: readingSettings.opened
            enabled: !root.readerWorkspace.showingPdf
            onClicked: readingSettings.opened
                           ? readingSettings.close()
                           : readingSettings.open()
        }

        LeaflitIconButton {
            symbol: root.darkMode ? "\u2600" : "\u263e"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: root.darkMode ? qsTr("Use light theme") : qsTr("Use dark theme")
            onChrome: true
            onClicked: root.darkModeToggled(!root.darkMode)
        }
    }

    ReadingSettingsPopup {
        id: readingSettings

        parent: root
        x: Math.max(Theme.spaceMd, root.width - width - Theme.spaceLg)
        y: root.height + Theme.spaceXs
        settingsStore: root.settingsStore
    }
}
