import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
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
    color: rowMouseArea.containsMouse ? Theme.surfaceMutedColor : Theme.surfaceColor
    radius: Theme.radiusMd
    border.color: activeFocus || keyboardCurrent || selected
                  ? Theme.focusColor : Theme.borderColor
    border.width: activeFocus || keyboardCurrent || selected ? 2 : 1
    opacity: fileAvailable ? 1 : 0.72
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

    Behavior on color {
        ColorAnimation {
            duration: Theme.motionFast
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

    MouseArea {
        id: rowMouseArea

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

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.spaceXs
        spacing: Theme.spaceSm

        LibraryBookCover {
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            title: root.title
            formatName: root.formatName
            coverUrl: root.coverUrl
            fallbackColor: root.fallbackColor
            compact: true
        }

        SZHCheckBox {
            visible: root.selectionMode
            checked: root.selected
            text: ""
            Accessible.name: qsTr("Select %1").arg(root.title)
            onClicked: root.selectionToggled(root.sourceUrl)
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space2xs

            Label {
                Layout.fillWidth: true
                text: root.title
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                font.weight: Font.DemiBold
                elide: Text.ElideRight
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
                            root.publicationYear > 0 ? String(root.publicationYear) : "",
                            root.collectionPath]
                         .filter(value => value.length > 0)
                         .join(qsTr("  \u00b7  "))
                         || Qt.formatDateTime(root.lastOpened, qsTr("dd MMM, HH:mm")))
                        : root.sourcePath
                color: root.fileAvailable ? Theme.mutedTextColor : Theme.dangerColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                elide: Text.ElideMiddle
            }
        }

        Label {
            Layout.preferredWidth: 68
            text: root.formatName
            color: Theme.mutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            visible: root.fileAvailable
            Layout.preferredWidth: 170
            spacing: Theme.spaceXs

            SZHProgressBar {
                Layout.fillWidth: true
                value: root.progress
                accessibleName: qsTr("Reading progress")
            }

            Label {
                Layout.preferredWidth: 34
                text: Math.round(root.progress * 100) + qsTr("%")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.captionFontSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignRight
            }
        }

        SZHButton {
            visible: !root.fileAvailable
            Layout.preferredWidth: 84
            variant: "secondary"
            text: qsTr("Locate")
            onClicked: root.relinkRequested(root.sourceUrl)
        }

        SZHIconButton {
            visible: !root.selectionMode
            symbol: "\u22ef"
            toolTip: qsTr("Book actions")
            onClicked: root.requestActions()
        }
    }
}
