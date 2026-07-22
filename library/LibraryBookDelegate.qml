import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property int index
    required property url sourceUrl
    required property string sourcePath
    required property string title
    required property string author
    required property string series
    required property real seriesNumber
    required property var genres
    required property var tags
    required property string language
    required property int publicationYear
    required property string formatName
    required property string collectionPath
    required property url coverUrl
    required property real progress
    required property date lastOpened
    required property bool fileAvailable
    required property bool cloudPlaceholder
    required property bool selected

    property color fallbackColor: Theme.accentColor
    property bool selectionMode: false
    property bool keyboardCurrent: false

    signal openRequested(url sourceUrl)
    signal relinkRequested(url sourceUrl)
    signal actionsRequested(url sourceUrl,
                            string title,
                            string collectionPath,
                            bool fileAvailable)
    signal selectionToggled(url sourceUrl)
    signal focusRequested(int row)

    function activate() {
        if (root.fileAvailable) {
            root.openRequested(root.sourceUrl)
        } else {
            root.relinkRequested(root.sourceUrl)
        }
    }

    function requestActions() {
        root.actionsRequested(root.sourceUrl,
                              root.title,
                              root.collectionPath,
                              root.fileAvailable)
    }

    activeFocusOnTab: true
    Accessible.role: Accessible.ListItem
    Accessible.name: root.title
    Accessible.description: root.fileAvailable
                                ? (root.cloudPlaceholder
                                   ? qsTr("Online-only book. Downloads when opened.")
                                   : qsTr("%1, %2% read")
                                     .arg(root.author.length > 0
                                          ? root.author : root.formatName)
                                     .arg(Math.round(root.progress * 100)))
                                : qsTr("File unavailable. Locate the book to continue.")
    Accessible.focusable: true
    Accessible.selected: root.selected || root.keyboardCurrent
    Accessible.onPressAction: root.activate()
    opacity: fileAvailable ? 1 : 0.72
    scale: cardMouseArea.containsMouse ? 1.008 : 1
    Drag.active: bookDragHandler.active && !root.selectionMode
    Drag.source: root
    Drag.keys: ["szhbooks-book"]
    Drag.dragType: Drag.Automatic
    Drag.mimeData: ({
        "application/x-szhbooks-book-url": root.sourceUrl.toString()
    })
    Drag.supportedActions: Qt.MoveAction
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2

    Behavior on scale {
        NumberAnimation {
            duration: Theme.motionFast
            easing.type: Easing.OutCubic
        }
    }

    Keys.onReturnPressed: root.selectionMode ? root.selectionToggled(root.sourceUrl) : root.activate()
    Keys.onEnterPressed: root.selectionMode ? root.selectionToggled(root.sourceUrl) : root.activate()
    Keys.onSpacePressed: root.selectionToggled(root.sourceUrl)
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Menu
                || (event.key === Qt.Key_F10
                    && (event.modifiers & Qt.ShiftModifier))) {
            root.requestActions()
            event.accepted = true
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: cardMouseArea.containsMouse ? Theme.surfaceMutedColor : Theme.surfaceColor
        radius: Theme.radiusMd
        border.color: root.activeFocus || root.keyboardCurrent || root.selected
                      ? Theme.focusColor : Theme.borderColor
        border.width: root.activeFocus || root.keyboardCurrent || root.selected ? 2 : 1

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
        cursorShape: Qt.PointingHandCursor
        onClicked: mouse => {
            root.focusRequested(root.index)
            if (root.selectionMode || (mouse.modifiers & Qt.ControlModifier)) {
                root.selectionToggled(root.sourceUrl)
            }
        }
        onDoubleClicked: {
            root.focusRequested(root.index)
            if (!root.selectionMode)
                root.activate()
        }
    }

    DragHandler {
        id: bookDragHandler

        target: null
        enabled: root.fileAvailable && !root.selectionMode
        acceptedButtons: Qt.LeftButton
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceSm
        spacing: Theme.spaceXs

        LibraryBookCover {
            Layout.fillWidth: true
            Layout.preferredHeight: 158
            title: root.title
            formatName: root.formatName
            coverUrl: root.coverUrl
            fallbackColor: root.fallbackColor
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
            text: root.cloudPlaceholder
                  ? qsTr("Online-only  \u00b7  Downloads when opened")
                  : root.fileAvailable
                    ? ([root.author,
                        root.series.length > 0
                          ? root.series + (root.seriesNumber > 0 ? " #" + root.seriesNumber : "")
                          : "",
                        root.publicationYear > 0 ? String(root.publicationYear) : ""]
                     .filter(value => value.length > 0).join(qsTr("  \u00b7  ")))
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
            visible: root.fileAvailable
            Layout.fillWidth: true
            spacing: Theme.spaceXs

            Label {
                text: Math.round(root.progress * 100) + qsTr("%")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                font.weight: Font.DemiBold
            }

            SZHProgressBar {
                Layout.fillWidth: true
                value: root.progress
                accessibleName: qsTr("Reading progress")
            }
        }

        SZHButton {
            visible: !root.fileAvailable
            Layout.fillWidth: true
            variant: "secondary"
            text: qsTr("Locate file")
            onClicked: root.relinkRequested(root.sourceUrl)
        }
    }

    SZHIconButton {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: Theme.spaceSm
        z: 2
        visible: !root.selectionMode
                 && (cardMouseArea.containsMouse
                     || root.activeFocus
                     || root.keyboardCurrent)
        symbol: "\u22ef"
        toolTip: qsTr("Book actions")
        onClicked: root.requestActions()
    }

    SZHCheckBox {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spaceSm
        z: 3
        visible: root.selectionMode
        checked: root.selected
        text: ""
        Accessible.name: qsTr("Select %1").arg(root.title)
        onClicked: root.selectionToggled(root.sourceUrl)
    }

    ToolTip {
        visible: cardMouseArea.containsMouse && !root.fileAvailable
        text: root.sourcePath
        delay: Theme.tooltipDelay
    }
}
