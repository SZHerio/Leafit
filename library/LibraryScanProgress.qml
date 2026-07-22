import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var libraryModel

    implicitHeight: root.libraryModel.scanning ? progressLayout.implicitHeight + Theme.spaceSm * 2 : 0
    visible: root.libraryModel.scanning
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
                    text: root.libraryModel.scanCancellationRequested
                          ? qsTr("Finishing library scan")
                          : qsTr("Updating book details")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    font.weight: Font.DemiBold
                }

                Label {
                    text: qsTr("%1 of %2")
                        .arg(root.libraryModel.scanCompleted)
                        .arg(root.libraryModel.scanTotal)
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                }
            }

            SZHProgressBar {
                Layout.fillWidth: true
                value: root.libraryModel.scanProgress
                accessibleName: qsTr("Library scan progress")
            }
        }

        SZHIconButton {
            symbol: "\u00d7"
            enabled: !root.libraryModel.scanCancellationRequested
            toolTip: qsTr("Cancel library scan")
            onClicked: root.libraryModel.cancelScan()
        }
    }
}
