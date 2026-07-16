pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var readerWorkspace
    required property var annotationStore
    property string activeTab: "contents"
    property string editingAnnotationId: ""
    property string editingExcerpt: ""

    readonly property bool editingExisting: editingAnnotationId.length > 0
    readonly property bool noteEditorActive: noteEditor.activeFocus
    readonly property var tabOptions: [
        { value: "contents", label: qsTr("Contents") },
        { value: "bookmarks", label: qsTr("Bookmarks") },
        { value: "notes", label: qsTr("Notes") }
    ]

    signal closeRequested

    function resetEditor() {
        root.editingAnnotationId = ""
        root.editingExcerpt = ""
        noteEditor.text = ""
    }

    function editAnnotation(annotation) {
        root.activeTab = "notes"
        root.editingAnnotationId = annotation.id
        root.editingExcerpt = annotation.excerpt
        noteEditor.text = annotation.note
        Qt.callLater(function() {
            noteEditor.forceActiveFocus()
        })
    }

    function focusSelectionNote() {
        root.activeTab = "notes"
        root.resetEditor()
        Qt.callLater(function() {
            noteEditor.forceActiveFocus()
        })
    }

    function saveNote() {
        if (root.editingExisting) {
            root.annotationStore.updateNote(root.editingAnnotationId, noteEditor.text)
        } else if (root.readerWorkspace.canCreateHighlight) {
            root.readerWorkspace.addSelectionHighlight(noteEditor.text)
        }
        root.resetEditor()
    }

    color: Theme.surfaceColor
    border.color: Theme.borderColor
    border.width: 1
    clip: true

    onActiveTabChanged: {
        if (activeTab !== "notes") {
            resetEditor()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceMd
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSm

            Label {
                Layout.fillWidth: true
                text: qsTr("Book navigation")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyLargeFontSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }

            Label {
                visible: root.activeTab !== "contents"
                text: root.annotationStore.totalCount
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
            }

            SZHIconButton {
                symbol: "\u00d7"
                toolTip: qsTr("Close sidebar")
                onClicked: root.closeRequested()
            }
        }

        SZHSegmentedControl {
            Layout.fillWidth: true
            model: root.tabOptions
            value: root.activeTab
            onValueSelected: value => root.activeTab = value
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.activeTab === "contents" ? 0
                          : root.activeTab === "bookmarks" ? 1 : 2

            Item {
                ListView {
                    id: chapterList

                    anchors.fill: parent
                    visible: root.readerWorkspace.hasChapters
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    currentIndex: root.readerWorkspace.currentChapterIndex
                    model: root.readerWorkspace.chapters
                    ScrollBar.vertical: SZHScrollBar {
                    }

                    delegate: Item {
                        id: chapterRow

                        required property int index
                        required property var modelData

                        width: chapterList.width
                        height: 48

                        Rectangle {
                            anchors.fill: parent
                            color: chapterMouse.containsMouse
                                   ? Theme.surfaceMutedColor
                                   : chapterRow.index === root.readerWorkspace.currentChapterIndex
                                     ? Theme.accentSoftColor
                                     : "transparent"
                            radius: Theme.radiusSm
                        }

                        MouseArea {
                            id: chapterMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.readerWorkspace.goToChapter(chapterRow.index)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.spaceSm
                            anchors.rightMargin: Theme.spaceSm
                            spacing: Theme.spaceSm

                            Label {
                                Layout.fillWidth: true
                                text: chapterRow.modelData.title
                                color: Theme.textColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.bodyFontSize
                                font.weight: chapterRow.index
                                             === root.readerWorkspace.currentChapterIndex
                                             ? Font.DemiBold : Font.Normal
                                elide: Text.ElideRight
                            }

                            Label {
                                text: Math.round(chapterRow.modelData.progress * 100) + qsTr("%")
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                            }
                        }
                    }
                }

                Label {
                    anchors.fill: parent
                    visible: !root.readerWorkspace.hasChapters
                    text: qsTr("This book has no table of contents")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }
            }

            ColumnLayout {
                spacing: Theme.spaceSm

                SZHButton {
                    Layout.fillWidth: true
                    variant: root.readerWorkspace.currentLocationBookmarked
                             ? "secondary" : "primary"
                    symbol: root.readerWorkspace.currentLocationBookmarked ? "\u2212" : "+"
                    text: root.readerWorkspace.currentLocationBookmarked
                          ? qsTr("Remove bookmark here")
                          : qsTr("Bookmark current location")
                    onClicked: root.readerWorkspace.toggleCurrentBookmark()
                }

                ListView {
                    id: bookmarkList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: Theme.space2xs
                    model: root.annotationStore.bookmarks
                    ScrollBar.vertical: SZHScrollBar {
                    }

                    delegate: Item {
                        id: bookmarkRow

                        required property var modelData

                        width: bookmarkList.width
                        height: 64

                        Rectangle {
                            anchors.fill: parent
                            color: bookmarkMouse.containsMouse
                                   ? Theme.surfaceMutedColor : "transparent"
                            radius: Theme.radiusSm
                        }

                        MouseArea {
                            id: bookmarkMouse

                            anchors.fill: parent
                            anchors.rightMargin: deleteBookmark.width + Theme.spaceSm
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.readerWorkspace.goToAnnotation(bookmarkRow.modelData)
                        }

                        ColumnLayout {
                            anchors.left: parent.left
                            anchors.right: deleteBookmark.left
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.spaceSm
                            anchors.rightMargin: Theme.spaceSm
                            spacing: Theme.space2xs

                            Label {
                                Layout.fillWidth: true
                                text: bookmarkRow.modelData.label.length > 0
                                      ? bookmarkRow.modelData.label
                                      : qsTr("%1%").arg(
                                            Math.round(bookmarkRow.modelData.progress * 100))
                                color: Theme.textColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.bodyFontSize
                                elide: Text.ElideRight
                            }

                            Label {
                                text: bookmarkRow.modelData.page >= 0
                                      ? qsTr("Page %1").arg(bookmarkRow.modelData.page + 1)
                                      : qsTr("%1% read").arg(
                                            Math.round(bookmarkRow.modelData.progress * 100))
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                            }
                        }

                        SZHIconButton {
                            id: deleteBookmark

                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spaceXs
                            anchors.verticalCenter: parent.verticalCenter
                            symbol: "\u00d7"
                            toolTip: qsTr("Delete bookmark")
                            onClicked: root.annotationStore.removeAnnotation(
                                           bookmarkRow.modelData.id)
                        }

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 1
                            color: Theme.borderColor
                        }
                    }
                }

                Label {
                    visible: root.annotationStore.bookmarks.length === 0
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("No bookmarks in this book")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }
            }

            ColumnLayout {
                spacing: Theme.spaceSm

                Rectangle {
                    visible: root.editingExisting
                             || root.readerWorkspace.canCreateHighlight
                    Layout.fillWidth: true
                    Layout.preferredHeight: noteEditorColumn.implicitHeight + Theme.spaceMd * 2
                    color: Theme.surfaceMutedColor
                    radius: Theme.radiusSm

                    ColumnLayout {
                        id: noteEditorColumn

                        anchors.fill: parent
                        anchors.margins: Theme.spaceMd
                        spacing: Theme.spaceXs

                        Label {
                            Layout.fillWidth: true
                            text: root.editingExisting
                                  ? root.editingExcerpt
                                  : root.readerWorkspace.selectedText
                            color: Theme.textColor
                            font.family: Theme.readingFontFamily
                            font.pixelSize: Theme.bodyFontSize
                            wrapMode: Text.Wrap
                            maximumLineCount: 3
                            elide: Text.ElideRight
                        }

                        SZHTextArea {
                            id: noteEditor

                            Layout.fillWidth: true
                            Layout.preferredHeight: 76
                            placeholderText: qsTr("Add a note")
                        }

                        RowLayout {
                            Layout.fillWidth: true

                            Item {
                                Layout.fillWidth: true
                            }

                            SZHButton {
                                variant: "ghost"
                                text: qsTr("Cancel")
                                onClicked: {
                                    if (!root.editingExisting) {
                                        root.readerWorkspace.clearSelection()
                                    }
                                    root.resetEditor()
                                }
                            }

                            SZHButton {
                                text: root.editingExisting ? qsTr("Save") : qsTr("Add note")
                                onClicked: root.saveNote()
                            }
                        }
                    }
                }

                ListView {
                    id: noteList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    spacing: Theme.space2xs
                    model: root.annotationStore.highlights
                    ScrollBar.vertical: SZHScrollBar {
                    }

                    delegate: Item {
                        id: noteRow

                        required property var modelData

                        width: noteList.width
                        height: modelData.note.length > 0 ? 112 : 88

                        Rectangle {
                            anchors.fill: parent
                            color: noteMouse.containsMouse
                                   ? Theme.surfaceMutedColor : "transparent"
                            radius: Theme.radiusSm
                        }

                        MouseArea {
                            id: noteMouse

                            anchors.fill: parent
                            anchors.rightMargin: noteActions.width + Theme.spaceSm
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.readerWorkspace.goToAnnotation(noteRow.modelData)
                        }

                        ColumnLayout {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: noteActions.left
                            anchors.margins: Theme.spaceSm
                            spacing: Theme.space2xs

                            Label {
                                Layout.fillWidth: true
                                text: noteRow.modelData.page >= 0
                                      ? qsTr("Page %1").arg(noteRow.modelData.page + 1)
                                      : qsTr("%1%").arg(
                                            Math.round(noteRow.modelData.progress * 100))
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: noteRow.modelData.excerpt
                                color: Theme.textColor
                                font.family: Theme.readingFontFamily
                                font.pixelSize: Theme.bodyFontSize
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }

                            Label {
                                visible: noteRow.modelData.note.length > 0
                                Layout.fillWidth: true
                                text: noteRow.modelData.note
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                                maximumLineCount: 2
                                wrapMode: Text.Wrap
                                elide: Text.ElideRight
                            }
                        }

                        Row {
                            id: noteActions

                            anchors.right: parent.right
                            anchors.rightMargin: Theme.spaceXs
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space2xs

                            SZHIconButton {
                                symbol: "\u270e"
                                toolTip: qsTr("Edit note")
                                onClicked: root.editAnnotation(noteRow.modelData)
                            }

                            SZHIconButton {
                                symbol: "\u00d7"
                                toolTip: qsTr("Delete note")
                                onClicked: root.annotationStore.removeAnnotation(
                                               noteRow.modelData.id)
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
                }

                Label {
                    visible: root.annotationStore.highlights.length === 0
                             && !root.readerWorkspace.canCreateHighlight
                             && !root.editingExisting
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: qsTr("No highlights or notes in this book")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
