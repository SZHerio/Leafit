pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property var syncService
    property string selectedCollection: ""

    signal collectionSelected(string collectionPath)
    signal createCollectionRequested(string parentPath)
    signal bookDropped(url sourceUrl, string collectionPath)

    Accessible.role: Accessible.Pane
    Accessible.name: qsTr("Library folders")

    function collectionOptions() {
        var options = [
            {
                "value": "",
                "label": qsTr("All books"),
                "depth": 0
            },
            {
                "value": ".",
                "label": qsTr("Library root"),
                "depth": 0
            }
        ]
        const collections = root.syncService.collections
        for (var index = 0; index < collections.length; ++index) {
            const parts = collections[index].split("/")
            options.push({
                "value": collections[index],
                "label": parts[parts.length - 1],
                "depth": parts.length - 1
            })
        }
        return options
    }

    function selectCollectionAt(index) {
        const options = root.collectionOptions()
        if (index >= 0 && index < options.length)
            root.collectionSelected(options[index].value)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: 1
            color: Theme.borderColor
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: Theme.spaceMd
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Label {
                Layout.fillWidth: true
                text: qsTr("Folders")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                font.weight: Font.DemiBold
            }

            SZHIconButton {
                symbol: "+"
                enabled: root.syncService.available && !root.syncService.fileService.busy
                toolTip: qsTr("New folder")
                onClicked: root.createCollectionRequested(root.selectedCollection === "." ? "" : root.selectedCollection)
            }
        }

        ListView {
            id: collectionList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space2xs
            model: root.collectionOptions()
            activeFocusOnTab: true
            keyNavigationEnabled: true
            Accessible.role: Accessible.List
            Accessible.name: qsTr("Library folders")
            ScrollBar.vertical: SZHScrollBar {}

            Keys.onReturnPressed: {
                root.selectCollectionAt(currentIndex)
            }
            Keys.onEnterPressed: {
                root.selectCollectionAt(currentIndex)
            }
            Keys.onSpacePressed: {
                root.selectCollectionAt(currentIndex)
            }

            delegate: Rectangle {
                id: collectionDelegate

                required property int index
                required property var modelData
                readonly property bool keyboardCurrent: ListView.isCurrentItem
                                                        && collectionList.activeFocus

                width: ListView.view.width
                height: Theme.compactControlHeight
                activeFocusOnTab: true
                Accessible.role: Accessible.ListItem
                Accessible.name: collectionDelegate.modelData.label
                Accessible.selected: root.selectedCollection
                                     === collectionDelegate.modelData.value
                Accessible.focusable: true
                Accessible.onPressAction: root.collectionSelected(
                                              collectionDelegate.modelData.value)
                color: collectionDropArea.containsDrag
                       || collectionDelegate.activeFocus
                       || collectionDelegate.keyboardCurrent
                       || root.selectedCollection === collectionDelegate.modelData.value
                       ? Theme.surfaceMutedColor : "transparent"
                radius: Theme.radiusSm
                border.color: collectionDropArea.containsDrag
                              || collectionDelegate.activeFocus
                              || collectionDelegate.keyboardCurrent
                              ? Theme.focusColor : "transparent"
                border.width: collectionDropArea.containsDrag
                              || collectionDelegate.activeFocus
                              || collectionDelegate.keyboardCurrent ? 2 : 0

                Keys.onReturnPressed: root.collectionSelected(
                                          collectionDelegate.modelData.value)
                Keys.onEnterPressed: root.collectionSelected(
                                         collectionDelegate.modelData.value)
                Keys.onSpacePressed: root.collectionSelected(
                                         collectionDelegate.modelData.value)

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: Theme.spaceSm
                                        + Math.min(3,
                                                   collectionDelegate.modelData.depth)
                                          * Theme.spaceSm
                    anchors.rightMargin: Theme.spaceXs
                    text: collectionDelegate.modelData.label
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    font.weight: root.selectedCollection
                                 === collectionDelegate.modelData.value
                                 ? Font.DemiBold : Font.Normal
                    elide: Text.ElideRight
                }

                TapHandler {
                    onTapped: {
                        collectionDelegate.forceActiveFocus(Qt.MouseFocusReason)
                        root.collectionSelected(collectionDelegate.modelData.value)
                    }
                }

                DropArea {
                    id: collectionDropArea

                    anchors.fill: parent
                    keys: ["szhbooks-book"]
                    onDropped: drop => {
                        const sourceUrl = drop.getDataAsString(
                                            "application/x-szhbooks-book-url")
                        if (!sourceUrl)
                            return
                        const destination = collectionDelegate.modelData.value
                                            === "." ? ""
                                                    : collectionDelegate.modelData.value
                        root.bookDropped(sourceUrl, destination)
                        drop.accept(Qt.MoveAction)
                    }
                }
            }
        }
    }
}
