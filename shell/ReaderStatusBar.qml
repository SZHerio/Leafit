import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var readerController
    required property var readerWorkspace

    readonly property bool showProgress: readerController.hasDocument
                                                 && readerController.errorMessage.length === 0
    readonly property string progressText: {
        if (!showProgress) {
            return ""
        }
        if (readerWorkspace.showingPdf) {
            return readerWorkspace.pageCount > 0
                    ? qsTr("Page %1 of %2")
                        .arg(readerWorkspace.currentPage + 1)
                        .arg(readerWorkspace.pageCount)
                    : qsTr("Loading PDF")
        }
        return Math.round(readerWorkspace.readingProgress * 100) + qsTr("%")
    }

    implicitHeight: Theme.statusBarHeight
    color: Theme.surfaceColor
    border.color: Theme.borderColor
    border.width: 1

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
            visible: root.showProgress
            text: root.readerController.formatName
            color: Theme.accentColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            font.weight: Font.DemiBold
        }

        Label {
            Layout.fillWidth: true
            text: root.readerController.errorMessage.length > 0
                      ? root.readerController.errorMessage
                      : root.readerController.sourcePath.length > 0
                        ? root.readerController.sourcePath
                        : qsTr("No file open")
            color: root.readerController.errorMessage.length > 0
                       ? Theme.dangerColor
                       : Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            elide: Text.ElideMiddle
            verticalAlignment: Text.AlignVCenter
        }

        LeaflitProgressBar {
            visible: root.showProgress
            Layout.preferredWidth: root.width < 900 ? 120 : 220
            value: root.readerWorkspace.readingProgress
        }

        Label {
            visible: root.showProgress
            Layout.preferredWidth: root.readerWorkspace.showingPdf ? 104 : 42
            text: root.progressText
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignVCenter
        }
    }
}
