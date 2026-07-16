pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Popup {
    id: root

    required property var readerWorkspace
    required property var annotationStore

    property string activeTab: "bookmarks"
    property string editingAnnotationId: ""
    property string editingExcerpt: ""
    readonly property bool editingExisting: editingAnnotationId.length > 0
    readonly property bool composingHighlight: activeTab === "highlights"
                                                 && (editingExisting
                                                     || readerWorkspace.canCreateHighlight)
    readonly property var activeModel: activeTab === "bookmarks"
                                       ? annotationStore.bookmarks
                                       : annotationStore.highlights
    readonly property var tabOptions: [
        { value: "bookmarks", label: qsTr("Bookmarks") },
        { value: "highlights", label: qsTr("Highlights") }
    ]
    readonly property real hostWindowHeight: root.parent && root.parent.Window.window
                                                   ? root.parent.Window.window.height
                                                   : 720
    readonly property real maximumPanelHeight: Math.max(320,
                                                         hostWindowHeight
                                                         - root.y
                                                         - Theme.spaceMd)

    function resetEditor() {
        root.editingAnnotationId = ""
        root.editingExcerpt = ""
        noteEditor.text = ""
    }

    function editAnnotation(annotation) {
        root.activeTab = "highlights"
        root.editingAnnotationId = annotation.id
        root.editingExcerpt = annotation.excerpt
        noteEditor.text = annotation.note
        Qt.callLater(function() {
            noteEditor.forceActiveFocus()
        })
    }

    function saveHighlightOrNote() {
        if (root.editingExisting) {
            root.annotationStore.updateNote(root.editingAnnotationId, noteEditor.text)
        } else {
            root.readerWorkspace.addSelectionHighlight(noteEditor.text)
        }
        root.resetEditor()
    }

    width: Math.min(396,
                    Math.max(320,
                             (parent ? parent.width : 428) - Theme.spaceXl))
    height: Math.min(root.activeModel.length === 0 && !root.composingHighlight
                     ? 360
                     : 500,
                     maximumPanelHeight)
    padding: Theme.spaceMd
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onActiveTabChanged: resetEditor()
    onClosed: resetEditor()

    background: Rectangle {
        color: Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: Theme.borderColor
        border.width: 1
    }

    contentItem: ColumnLayout {
        spacing: Theme.spaceSm

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                text: qsTr("Reading marks")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyLargeFontSize
                font.weight: Font.DemiBold
            }

            Label {
                text: root.annotationStore.totalCount
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
            }
        }

        SZHSegmentedControl {
            Layout.fillWidth: true
            model: root.tabOptions
            value: root.activeTab
            onValueSelected: value => root.activeTab = value
        }

        SZHButton {
            visible: root.activeTab === "bookmarks"
            Layout.fillWidth: true
            variant: root.readerWorkspace.currentLocationBookmarked
                     ? "secondary"
                     : "primary"
            symbol: root.readerWorkspace.currentLocationBookmarked ? "\u2212" : "+"
            text: root.readerWorkspace.currentLocationBookmarked
                  ? qsTr("Remove bookmark here")
                  : qsTr("Bookmark current location")
            onClicked: root.readerWorkspace.toggleCurrentBookmark()
        }

        Rectangle {
            visible: root.composingHighlight
            Layout.fillWidth: true
            Layout.preferredHeight: editorColumn.implicitHeight + Theme.spaceMd * 2
            color: Theme.surfaceMutedColor
            radius: Theme.radiusSm

            ColumnLayout {
                id: editorColumn

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
                    Layout.preferredHeight: 72
                    placeholderText: qsTr("Add a note (optional)")
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
                            root.readerWorkspace.clearSelection()
                            root.resetEditor()
                        }
                    }

                    SZHButton {
                        text: root.editingExisting ? qsTr("Save note") : qsTr("Save highlight")
                        onClicked: root.saveHighlightOrNote()
                    }
                }
            }
        }

        Label {
            visible: root.activeTab === "highlights" && !root.composingHighlight
            Layout.fillWidth: true
            text: qsTr("No active text selection")
            color: Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            wrapMode: Text.Wrap
        }

        ListView {
            id: annotationList

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            spacing: Theme.space2xs
            model: root.activeModel
            ScrollBar.vertical: SZHScrollBar {
            }

            delegate: Item {
                id: annotationRow

                required property var modelData

                width: annotationList.width
                height: modelData.note && modelData.note.length > 0 ? 104 : 80

                Rectangle {
                    anchors.fill: parent
                    color: rowMouse.containsMouse ? Theme.surfaceMutedColor : "transparent"
                    radius: Theme.radiusSm
                }

                MouseArea {
                    id: rowMouse

                    anchors.fill: parent
                    anchors.rightMargin: rowActions.width + Theme.spaceSm
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.readerWorkspace.goToAnnotation(annotationRow.modelData)
                }

                ColumnLayout {
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.right: rowActions.left
                    anchors.margins: Theme.spaceSm
                    spacing: Theme.space2xs

                    Label {
                        Layout.fillWidth: true
                        text: annotationRow.modelData.page >= 0
                              ? qsTr("Page %1").arg(annotationRow.modelData.page + 1)
                              : annotationRow.modelData.label.length > 0
                                ? annotationRow.modelData.label
                                : qsTr("%1%").arg(Math.round(annotationRow.modelData.progress * 100))
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: annotationRow.modelData.type === "highlight"
                        Layout.fillWidth: true
                        text: annotationRow.modelData.excerpt
                        color: Theme.textColor
                        font.family: Theme.readingFontFamily
                        font.pixelSize: Theme.bodyFontSize
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }

                    Label {
                        visible: annotationRow.modelData.note.length > 0
                        Layout.fillWidth: true
                        text: annotationRow.modelData.note
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                }

                Row {
                    id: rowActions

                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spaceXs
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space2xs

                    SZHIconButton {
                        visible: annotationRow.modelData.type === "highlight"
                        symbol: "\u270e"
                        toolTip: qsTr("Edit note")
                        onClicked: root.editAnnotation(annotationRow.modelData)
                    }

                    SZHIconButton {
                        symbol: "\u00d7"
                        toolTip: qsTr("Delete")
                        onClicked: root.annotationStore.removeAnnotation(annotationRow.modelData.id)
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
            visible: root.activeModel.length === 0
                     && !(root.activeTab === "highlights" && root.composingHighlight)
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: root.activeTab === "bookmarks"
                  ? qsTr("No bookmarks in this book")
                  : qsTr("No highlights or notes in this book")
            color: Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Theme.motionFast
            }
            NumberAnimation {
                property: "scale"
                from: 0.98
                to: 1
                duration: Theme.motionFast
            }
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: Theme.motionFast
        }
    }
}
