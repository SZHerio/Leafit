import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property int index
    required property url sourceUrl
    required property string sourcePath
    required property string title
    required property string formatName
    required property real progress
    required property date lastOpened
    required property bool fileAvailable

    property color coverColor: Theme.accentColor

    signal openRequested(url sourceUrl)
    signal removeRequested(int row)

    activeFocusOnTab: true
    opacity: fileAvailable ? 1 : 0.68

    Keys.onReturnPressed: if (root.fileAvailable) root.openRequested(root.sourceUrl)
    Keys.onEnterPressed: if (root.fileAvailable) root.openRequested(root.sourceUrl)

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: cardMouseArea.containsMouse ? Theme.surfaceMutedColor : Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: root.activeFocus ? Theme.focusColor : Theme.borderColor
        border.width: root.activeFocus ? 2 : 1

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionFast
            }
        }
    }

    MouseArea {
        id: cardMouseArea

        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.fileAvailable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: {
            root.forceActiveFocus()
            if (root.fileAvailable) {
                root.openRequested(root.sourceUrl)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceSm
        spacing: Theme.spaceXs

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 154
            color: root.coverColor
            radius: Theme.radiusSm
            clip: true

            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 8
                color: Qt.darker(root.coverColor, 1.25)
            }

            Label {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.spaceSm
                text: root.formatName
                color: "#ffffff"
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                font.weight: Font.DemiBold
            }

            Text {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Theme.spaceLg
                text: root.title
                color: "#ffffff"
                font.family: Theme.readingFontFamily
                font.pixelSize: Theme.bodyLargeFontSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }
        }

        Label {
            Layout.fillWidth: true
            text: root.title
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            font.weight: Font.DemiBold
            maximumLineCount: 2
            elide: Text.ElideRight
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            text: root.fileAvailable
                  ? Qt.formatDateTime(root.lastOpened, qsTr("dd MMM, HH:mm"))
                  : qsTr("File unavailable")
            color: root.fileAvailable ? Theme.mutedTextColor : Theme.dangerColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            elide: Text.ElideRight
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Label {
                text: Math.round(root.progress * 100) + qsTr("%")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                font.weight: Font.DemiBold
            }

            LeaflitProgressBar {
                Layout.fillWidth: true
                value: root.progress
            }
        }
    }

    LeaflitIconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spaceSm
        z: 2
        visible: cardMouseArea.containsMouse || root.activeFocus
        symbol: "\u00d7"
        toolTip: qsTr("Remove from library")
        onClicked: root.removeRequested(root.index)
    }

    ToolTip {
        visible: cardMouseArea.containsMouse && !root.fileAvailable
        text: root.sourcePath
        delay: Theme.tooltipDelay
    }
}
