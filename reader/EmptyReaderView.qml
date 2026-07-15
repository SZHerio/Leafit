import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string errorMessage: ""
    signal openRequested

    Column {
        anchors.centerIn: parent
        width: Math.min(420, Math.max(0, parent.width - Theme.space2xl * 2))
        spacing: Theme.spaceMd

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 52
            height: 52
            radius: Theme.radiusLg
            color: Theme.accentSoftColor

            Label {
                anchors.centerIn: parent
                text: qsTr("L")
                color: Theme.accentColor
                font.family: Theme.readingFontFamily
                font.pixelSize: 26
                font.weight: Font.DemiBold
            }
        }

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.errorMessage.length > 0
                      ? qsTr("This book could not be opened")
                      : qsTr("No book open")
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.titleFontSize
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Label {
            visible: root.errorMessage.length > 0
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            text: root.errorMessage
            color: Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        LeaflitButton {
            anchors.horizontalCenter: parent.horizontalCenter
            topPadding: 0
            variant: "primary"
            text: root.errorMessage.length > 0 ? qsTr("Choose another book") : qsTr("Open book")
            onClicked: root.openRequested()
        }
    }
}
