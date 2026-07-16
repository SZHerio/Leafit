import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property bool shown: false
    property string heading: ""
    property string message: ""
    property string actionText: ""

    signal actionRequested
    signal dismissed

    implicitWidth: 460
    implicitHeight: notificationContent.implicitHeight + Theme.spaceMd * 2
    visible: root.shown || opacity > 0.01
    enabled: root.shown
    opacity: root.shown ? 1 : 0
    color: Theme.surfaceColor
    radius: Theme.radiusMd
    border.color: Theme.strongBorderColor
    border.width: 1

    transform: Translate {
        y: root.shown ? 0 : -Theme.spaceSm

        Behavior on y {
            NumberAnimation {
                duration: Theme.motionNormal
                easing.type: Easing.OutCubic
            }
        }
    }

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.motionNormal
        }
    }

    RowLayout {
        id: notificationContent

        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceSm

        Label {
            text: "!"
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.titleFontSize
            font.weight: Font.Bold
            verticalAlignment: Text.AlignTop
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2xs

            Label {
                Layout.fillWidth: true
                text: root.heading
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }

            Label {
                Layout.fillWidth: true
                text: root.message
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                wrapMode: Text.Wrap
                maximumLineCount: 3
                elide: Text.ElideRight
            }
        }

        SZHButton {
            visible: root.actionText.length > 0
            variant: "secondary"
            text: root.actionText
            onClicked: root.actionRequested()
        }

        SZHIconButton {
            symbol: "\u00d7"
            toolTip: qsTr("Dismiss")
            onClicked: root.dismissed()
        }
    }
}
