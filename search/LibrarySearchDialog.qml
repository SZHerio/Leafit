pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property var searchModel
    readonly property bool queryReady: queryField.text.trim().length >= 2

    signal resultActivated(var result)

    function openResult(index) {
        const result = searchModel.get(index)
        if (!result || result.sourceUrl === undefined)
            return
        resultActivated(result)
        close()
    }

    function indexStatusText() {
        if (searchModel.indexing) {
            return searchModel.cancellationRequested
                   ? qsTr("Finishing the current book...")
                   : qsTr("Indexing %1 of %2 books")
                     .arg(searchModel.indexCompleted)
                     .arg(searchModel.totalBooks)
        }
        if (searchModel.totalBooks === 0)
            return qsTr("Add books to search their contents")
        if (searchModel.failedBooks > 0) {
            return qsTr("%1 of %2 books indexed, %3 skipped")
                .arg(searchModel.indexedBooks)
                .arg(searchModel.totalBooks)
                .arg(searchModel.failedBooks)
        }
        return qsTr("%1 of %2 books indexed")
            .arg(searchModel.indexedBooks)
            .arg(searchModel.totalBooks)
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(820, parent ? parent.width - Theme.space2xl : 820)
    height: Math.min(650, parent ? parent.height - Theme.space2xl : 650)
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    onOpened: {
        searchModel.clearError()
        queryField.text = searchModel.query
        searchModel.ensureIndexed()
        resultList.currentIndex = -1
        queryField.forceActiveFocus()
    }

    background: Rectangle {
        color: Theme.surfaceColor
        border.color: Theme.borderColor
        border.width: 1
        radius: Theme.radiusLg
    }

    header: Item {
        implicitHeight: 64

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spaceLg
            anchors.rightMargin: Theme.spaceMd
            spacing: Theme.spaceMd

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space2xs

                Label {
                    text: qsTr("Search library")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.titleFontSize
                    font.weight: Font.DemiBold
                }

                Label {
                    Layout.fillWidth: true
                    text: root.indexStatusText()
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                    elide: Text.ElideRight
                }

                SZHProgressBar {
                    Layout.fillWidth: true
                    visible: root.searchModel.indexing
                    value: root.searchModel.indexProgress
                    accessibleName: qsTr("Search indexing progress")
                }
            }

            SZHIconButton {
                visible: root.searchModel.indexing
                symbol: "\u00d7"
                enabled: !root.searchModel.cancellationRequested
                toolTip: qsTr("Cancel indexing")
                onClicked: root.searchModel.cancelIndexing()
            }

            SZHIconButton {
                objectName: "rebuildSearchIndexButton"
                visible: !root.searchModel.indexing
                symbol: "\u21bb"
                toolTip: qsTr("Rebuild search index")
                enabled: !root.searchModel.indexing
                onClicked: root.searchModel.rebuildIndex()
            }

            SZHIconButton {
                symbol: "\u00d7"
                toolTip: qsTr("Close")
                onClicked: root.close()
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.borderColor
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: 72

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spaceLg
                anchors.rightMargin: Theme.spaceLg
                spacing: Theme.spaceMd

                SZHTextField {
                    id: queryField

                    objectName: "librarySearchField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search every book")
                    onTextChanged: searchDelay.restart()

                    Keys.onReturnPressed: {
                        if (root.searchModel.count > 0)
                            root.openResult(0)
                    }
                    Keys.onDownPressed: {
                        if (root.searchModel.count > 0) {
                            resultList.currentIndex = 0
                            if (resultList.currentItem)
                                resultList.currentItem.forceActiveFocus()
                        }
                    }
                }

                Label {
                    Layout.preferredWidth: 180
                    text: root.queryReady
                          ? qsTr("%n result(s)", "", root.searchModel.count)
                          : qsTr("Enter 2+ characters")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                    horizontalAlignment: Text.AlignRight
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.borderColor
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceLg
            Layout.rightMargin: Theme.spaceLg
            Layout.topMargin: Theme.spaceSm
            visible: root.searchModel.errorMessage.length > 0
            text: root.searchModel.errorMessage
            color: Theme.dangerColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            wrapMode: Text.Wrap
        }

        ListView {
            id: resultList

            objectName: "librarySearchResults"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: Theme.spaceSm
            clip: true
            spacing: Theme.space2xs
            model: root.searchModel
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: SZHScrollBar {}

            delegate: Rectangle {
                id: resultDelegate

                required property int index
                required property url sourceUrl
                required property string bookTitle
                required property string bookAuthor
                required property string formatName
                required property string snippet
                required property int start
                required property int page
                required property real progress

                width: ListView.view.width
                height: 116
                radius: Theme.radiusMd
                color: resultHover.hovered || activeFocus
                       ? Theme.surfaceMutedColor : "transparent"
                border.color: activeFocus ? Theme.strongBorderColor : "transparent"
                border.width: 1
                focus: resultList.currentIndex === index

                Accessible.role: Accessible.ListItem
                Accessible.name: bookTitle + ", " + snippet

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spaceSm
                    spacing: Theme.space2xs

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm

                        Label {
                            Layout.fillWidth: true
                            text: resultDelegate.bookTitle
                            color: Theme.textColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.bodyFontSize
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }

                        Label {
                            text: resultDelegate.page >= 0
                                  ? qsTr("Page %1").arg(resultDelegate.page + 1)
                                  : qsTr("%1%").arg(Math.round(
                                                        resultDelegate.progress * 100))
                            color: Theme.mutedTextColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.captionFontSize
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        text: resultDelegate.snippet.length > 0
                              ? resultDelegate.snippet
                              : qsTr("Match in book details")
                        color: Theme.textColor
                        font.family: Theme.readingFontFamily
                        font.pixelSize: Theme.bodyFontSize
                        wrapMode: Text.Wrap
                        maximumLineCount: 3
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: (resultDelegate.bookAuthor.length > 0
                               ? resultDelegate.bookAuthor + "  \u00b7  " : "")
                              + resultDelegate.formatName
                        color: Theme.disabledTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        elide: Text.ElideRight
                    }
                }

                HoverHandler { id: resultHover }

                TapHandler {
                    onTapped: {
                        resultList.currentIndex = resultDelegate.index
                        resultDelegate.forceActiveFocus()
                    }
                    onDoubleTapped: root.openResult(resultDelegate.index)
                }

                Keys.onReturnPressed: root.openResult(resultDelegate.index)
                Keys.onEnterPressed: root.openResult(resultDelegate.index)
            }

            Label {
                anchors.centerIn: parent
                width: Math.min(400, parent.width - Theme.space2xl)
                visible: root.searchModel.count === 0
                text: !root.queryReady
                      ? qsTr("Type a word or phrase to search the whole library")
                      : root.searchModel.indexing
                        ? qsTr("The index is updating. Results will appear here.")
                        : qsTr("No matches found")
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }

    Timer {
        id: searchDelay

        interval: 180
        repeat: false
        onTriggered: root.searchModel.query = queryField.text
    }
}
