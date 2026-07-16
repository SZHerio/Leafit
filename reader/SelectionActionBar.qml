import QtQuick
import QtQuick.Layouts

SZHSurface {
    id: root

    signal copyRequested
    signal highlightRequested
    signal noteRequested
    signal closeRequested

    implicitWidth: actions.implicitWidth + Theme.spaceMd * 2
    implicitHeight: Theme.controlHeight + Theme.spaceSm * 2
    width: implicitWidth
    height: implicitHeight
    fillColor: Theme.surfaceColor
    radius: Theme.radiusMd

    RowLayout {
        id: actions

        anchors.centerIn: parent
        spacing: Theme.space2xs

        SZHButton {
            text: qsTr("Copy")
            symbol: "\u2398"
            variant: "ghost"
            onClicked: root.copyRequested()
        }

        SZHButton {
            text: qsTr("Highlight")
            symbol: "\u25ac"
            variant: "ghost"
            onClicked: root.highlightRequested()
        }

        SZHButton {
            text: qsTr("Note")
            symbol: "+"
            variant: "ghost"
            onClicked: root.noteRequested()
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            color: Theme.borderColor
        }

        SZHIconButton {
            symbol: "\u00d7"
            toolTip: qsTr("Clear selection")
            onClicked: root.closeRequested()
        }
    }
}
