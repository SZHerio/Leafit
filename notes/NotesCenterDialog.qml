pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Dialog {
    id: root

    required property var notesModel
    property var selectedAnnotation: ({})
    property string exportFormat: "markdown"
    readonly property bool hasSelection: selectedAnnotation.annotationId !== undefined

    signal annotationActivated(var annotation)

    function clearSelection() {
        selectedAnnotation = ({})
        labelEditor.text = ""
        noteEditor.text = ""
    }

    function selectAnnotation(annotation) {
        selectedAnnotation = annotation || ({})
        labelEditor.text = selectedAnnotation.label || ""
        noteEditor.text = selectedAnnotation.note || ""
    }

    function selectFirstVisible() {
        if (notesModel.count > 0) {
            selectAnnotation(notesModel.get(0))
            annotationList.currentIndex = 0
        } else {
            clearSelection()
            annotationList.currentIndex = -1
        }
    }

    function applyFilter(filter) {
        notesModel.typeFilter = filter
        Qt.callLater(selectFirstVisible)
    }

    function saveSelection() {
        if (!hasSelection)
            return
        if (notesModel.updateAnnotation(selectedAnnotation.sourceUrl,
                                        selectedAnnotation.annotationId,
                                        labelEditor.text,
                                        noteEditor.text)) {
            const refreshed = notesModel.find(selectedAnnotation.sourceUrl,
                                               selectedAnnotation.annotationId)
            selectAnnotation(refreshed)
        }
    }

    function openSelection() {
        if (!hasSelection)
            return
        annotationActivated(selectedAnnotation)
        close()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(940, parent ? parent.width - Theme.space2xl : 940)
    height: Math.min(650, parent ? parent.height - Theme.space2xl : 650)
    modal: true
    closePolicy: Popup.CloseOnEscape
    padding: 0

    onOpened: {
        notesModel.clearError()
        notesModel.refresh()
        notesModel.query = ""
        notesModel.typeFilter = "all"
        queryField.text = ""
        typeSegments.value = "all"
        selectFirstVisible()
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
                    text: qsTr("Notes center")
                    color: Theme.textColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.titleFontSize
                    font.weight: Font.DemiBold
                }

                Label {
                    text: qsTr("%n reading mark(s)", "", root.notesModel.totalCount)
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.captionFontSize
                }
            }

            SZHMenuButton {
                id: exportFormatMenu

                objectName: "notesExportFormatMenu"
                Layout.preferredWidth: 154
                labelPrefix: qsTr("Export")
                value: root.exportFormat
                options: [
                    {"value": "markdown", "label": qsTr("Markdown")},
                    {"value": "html", "label": qsTr("HTML")}
                ]
                onValueSelected: value => root.exportFormat = value
            }

            SZHIconButton {
                objectName: "exportNotesButton"
                symbol: "\u21e9"
                toolTip: qsTr("Export visible notes")
                enabled: root.notesModel.count > 0
                onClicked: exportDialog.open()
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
            Layout.preferredHeight: 68

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spaceLg
                anchors.rightMargin: Theme.spaceLg
                spacing: Theme.spaceMd

                SZHTextField {
                    id: queryField

                    objectName: "notesSearchField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search notes and books")
                    onTextChanged: root.notesModel.query = text
                }

                SZHSegmentedControl {
                    id: typeSegments

                    objectName: "notesTypeFilter"
                    Layout.preferredWidth: 430
                    model: [
                        {"value": "all", "label": qsTr("All")},
                        {"value": "bookmarks", "label": qsTr("Bookmarks")},
                        {"value": "highlights", "label": qsTr("Highlights")},
                        {"value": "notes", "label": qsTr("Notes")}
                    ]
                    value: root.notesModel.typeFilter
                    onValueSelected: value => root.applyFilter(value)
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

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Item {
                SplitView.minimumWidth: 300
                SplitView.preferredWidth: 500

                ListView {
                    id: annotationList

                    objectName: "notesList"
                    anchors.fill: parent
                    anchors.margins: Theme.spaceSm
                    clip: true
                    spacing: Theme.space2xs
                    model: root.notesModel
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: SZHScrollBar {}

                    delegate: Rectangle {
                        id: annotationDelegate

                        required property int index
                        required property string annotationId
                        required property string annotationType
                        required property url sourceUrl
                        required property string bookTitle
                        required property string bookAuthor
                        required property string formatName
                        required property real progress
                        required property int page
                        required property int start
                        required property int length
                        required property string label
                        required property string excerpt
                        required property string note
                        required property date createdAt

                        width: ListView.view.width
                        height: 112
                        radius: Theme.radiusMd
                        color: annotationList.currentIndex === index
                               ? Theme.accentSoftColor
                               : delegateHover.hovered
                                 ? Theme.surfaceMutedColor : "transparent"
                        border.color: annotationList.currentIndex === index
                                      ? Theme.strongBorderColor : "transparent"
                        border.width: 1

                        Accessible.role: Accessible.ListItem
                        Accessible.name: bookTitle + ", "
                                         + (annotationType === "bookmark"
                                            ? qsTr("Bookmark") : qsTr("Highlight"))
                        focus: annotationList.currentIndex === index

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spaceSm
                            spacing: Theme.space2xs

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.spaceSm

                                Label {
                                    Layout.fillWidth: true
                                    text: annotationDelegate.bookTitle
                                    color: Theme.textColor
                                    font.family: Theme.uiFontFamily
                                    font.pixelSize: Theme.bodyFontSize
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: annotationDelegate.page >= 0
                                          ? qsTr("Page %1").arg(annotationDelegate.page + 1)
                                          : qsTr("%1%").arg(Math.round(
                                                                annotationDelegate.progress * 100))
                                    color: Theme.mutedTextColor
                                    font.family: Theme.uiFontFamily
                                    font.pixelSize: Theme.captionFontSize
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: annotationDelegate.label.length > 0
                                      ? annotationDelegate.label
                                      : annotationDelegate.excerpt.length > 0
                                        ? annotationDelegate.excerpt
                                        : annotationDelegate.annotationType === "bookmark"
                                          ? qsTr("Bookmark") : qsTr("Highlight")
                                color: Theme.textColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.bodyFontSize
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: annotationDelegate.note
                                visible: text.length > 0
                                color: Theme.mutedTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: (annotationDelegate.bookAuthor.length > 0
                                       ? annotationDelegate.bookAuthor + "  \u00b7  " : "")
                                      + annotationDelegate.formatName
                                color: Theme.disabledTextColor
                                font.family: Theme.uiFontFamily
                                font.pixelSize: Theme.captionFontSize
                                elide: Text.ElideRight
                            }
                        }

                        HoverHandler { id: delegateHover }

                        TapHandler {
                            onTapped: {
                                annotationList.currentIndex = annotationDelegate.index
                                root.selectAnnotation(root.notesModel.get(
                                                          annotationDelegate.index))
                            }
                            onDoubleTapped: root.openSelection()
                        }

                        Keys.onReturnPressed: root.openSelection()
                        Keys.onEnterPressed: root.openSelection()
                    }

                    Label {
                        anchors.centerIn: parent
                        width: Math.min(320, parent.width - Theme.spaceLg)
                        visible: root.notesModel.count === 0
                        text: root.notesModel.totalCount === 0
                              ? qsTr("No reading marks yet")
                              : qsTr("No notes match the current filter")
                        color: Theme.mutedTextColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.bodyFontSize
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item {
                SplitView.minimumWidth: 300
                SplitView.preferredWidth: 390

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: Theme.borderColor
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spaceLg
                    spacing: Theme.spaceMd
                    visible: root.hasSelection

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.space2xs

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedAnnotation.bookTitle || ""
                            color: Theme.textColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.bodyLargeFontSize
                            font.weight: Font.DemiBold
                            wrapMode: Text.Wrap
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedAnnotation.bookAuthor || ""
                            visible: text.length > 0
                            color: Theme.mutedTextColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.captionFontSize
                            elide: Text.ElideRight
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.selectedAnnotation.excerpt || ""
                        visible: text.length > 0
                        color: Theme.textColor
                        font.family: Theme.readingFontFamily
                        font.pixelSize: Theme.bodyFontSize
                        font.italic: true
                        wrapMode: Text.Wrap
                        maximumLineCount: 5
                        elide: Text.ElideRight
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXs

                        Label {
                            text: qsTr("Label")
                            color: Theme.mutedTextColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.captionFontSize
                        }

                        SZHTextField {
                            id: labelEditor

                            objectName: "annotationLabelEditor"
                            Layout.fillWidth: true
                            placeholderText: qsTr("Reading mark label")
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: Theme.spaceXs

                        Label {
                            text: qsTr("Note")
                            color: Theme.mutedTextColor
                            font.family: Theme.uiFontFamily
                            font.pixelSize: Theme.captionFontSize
                        }

                        SZHTextArea {
                            id: noteEditor

                            objectName: "annotationNoteEditor"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            placeholderText: qsTr("Write a note")
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: root.notesModel.errorMessage.length > 0
                        text: root.notesModel.errorMessage
                        color: Theme.dangerColor
                        font.family: Theme.uiFontFamily
                        font.pixelSize: Theme.captionFontSize
                        wrapMode: Text.Wrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceSm

                        SZHButton {
                            text: qsTr("Open")
                            onClicked: root.openSelection()
                        }

                        SZHButton {
                            objectName: "saveAnnotationButton"
                            text: qsTr("Save")
                            variant: "secondary"
                            onClicked: root.saveSelection()
                        }

                        Item { Layout.fillWidth: true }

                        SZHIconButton {
                            symbol: "\u232b"
                            toolTip: qsTr("Delete reading mark")
                            onClicked: deleteConfirmDialog.open()
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    width: Math.min(260, parent.width - Theme.spaceLg)
                    visible: !root.hasSelection
                    text: qsTr("Select a reading mark")
                    color: Theme.mutedTextColor
                    font.family: Theme.uiFontFamily
                    font.pixelSize: Theme.bodyFontSize
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }

    FileDialog {
        id: exportDialog

        title: qsTr("Export reading notes")
        fileMode: FileDialog.SaveFile
        defaultSuffix: root.exportFormat === "html" ? "html" : "md"
        nameFilters: root.exportFormat === "html"
                     ? [qsTr("HTML document (*.html)")]
                     : [qsTr("Markdown document (*.md)")]
        onAccepted: root.notesModel.exportFiltered(selectedFile, root.exportFormat)
    }

    Dialog {
        id: deleteConfirmDialog

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Delete reading mark?")
        standardButtons: Dialog.Cancel

        contentItem: Label {
            width: 320
            text: qsTr("This bookmark, highlight or note will be removed from every synchronized device.")
            color: Theme.textColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            wrapMode: Text.Wrap
        }

        footer: DialogButtonBox {
            standardButtons: DialogButtonBox.Cancel

            SZHButton {
                text: qsTr("Delete")
                onClicked: {
                    const sourceUrl = root.selectedAnnotation.sourceUrl
                    const annotationId = root.selectedAnnotation.annotationId
                    deleteConfirmDialog.close()
                    if (root.notesModel.removeAnnotation(sourceUrl, annotationId))
                        root.selectFirstVisible()
                }
            }
        }
    }
}
