import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    required property var readerWorkspace

    signal openRequested
    signal chaptersRequested
    signal settingsRequested

    width: Math.min(284,
                    Math.max(248,
                             (parent ? parent.width : 316) - Theme.spaceXl))
    padding: Theme.spaceMd
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
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Label {
                Layout.fillWidth: true
                text: root.readerWorkspace.showingPdf ? qsTr("Zoom") : qsTr("Text size")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                font.weight: Font.DemiBold
            }

            SZHIconButton {
                symbol: "-"
                toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom out")
                         : qsTr("Decrease font size")
                enabled: root.readerWorkspace.canDecreaseScale
                onClicked: root.readerWorkspace.decreaseScale()
            }

            Label {
                Layout.preferredWidth: 54
                text: root.readerWorkspace.scaleLabel
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            SZHIconButton {
                symbol: "+"
                toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom in")
                         : qsTr("Increase font size")
                enabled: root.readerWorkspace.canIncreaseScale
                onClicked: root.readerWorkspace.increaseScale()
            }
        }

        SZHButton {
            visible: root.readerWorkspace.showingPdf
            Layout.fillWidth: true
            variant: "secondary"
            symbol: "\u2922"
            text: qsTr("Fit to width")
            onClicked: root.readerWorkspace.fitPdfToWidth()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.borderColor
        }

        SZHButton {
            Layout.fillWidth: true
            variant: "ghost"
            symbol: "+"
            text: qsTr("Open book")
            onClicked: root.openRequested()
        }

        SZHButton {
            visible: root.readerWorkspace.hasChapters
            Layout.fillWidth: true
            variant: "ghost"
            symbol: "\u2630"
            text: qsTr("Chapters")
            onClicked: root.chaptersRequested()
        }

        SZHButton {
            Layout.fillWidth: true
            variant: "ghost"
            symbol: "\u2699"
            text: qsTr("Reading settings")
            onClicked: root.settingsRequested()
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.motionFast
            }
            NumberAnimation {
                property: "scale"
                from: 0.98
                to: 1
                duration: Theme.motionFast
            }
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
