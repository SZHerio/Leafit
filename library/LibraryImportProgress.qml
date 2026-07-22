import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var fileService

    implicitHeight: root.fileService.busy ? progressLayout.implicitHeight + Theme.spaceSm * 2 : 0
    visible: root.fileService.busy
    color: Theme.surfaceColor
    border.color: Theme.borderColor
    border.width: 1
    radius: Theme.radiusMd

    RowLayout {
        id: progressLayout

        anchors.fill: parent
        anchors.leftMargin: Theme.spaceMd
        anchors.rightMargin: Theme.spaceSm
        anchors.topMargin: Theme.spaceSm
        anchors.bottomMargin: Theme.spaceSm
        spacing: Theme.spaceSm

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2xs

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.spaceSm

                Label {
                    Layout.fillWidth: true
                    text: root.fileService.cancellationRequested ? qsTr("Finishing current file") : qsTr("Importing %1").arg(root.fileService.currentFileName)
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    font.weight: Font.DemiBold
                    elide: Text.ElideMiddle
                }

                Label {
                    text: qsTr("%1 of %2").arg(root.fileService.operationCompleted).arg(root.fileService.operationTotal)
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                }
            }

            SZHProgressBar {
                Layout.fillWidth: true
                value: root.fileService.progress
                accessibleName: qsTr("Book import progress")
            }
        }

        SZHIconButton {
            symbol: "\u00d7"
            enabled: !root.fileService.cancellationRequested
            toolTip: qsTr("Cancel import")
            onClicked: root.fileService.cancel()
        }
    }
}
