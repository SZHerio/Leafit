import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property string documentText
    required property var documentFormatter
    property int fontSize: 18
    property real lineHeight: 1.5
    property int preferredPageWidth: Theme.readerPageMaxWidth
    property int pagePadding: Theme.readerPagePadding
    property real pendingProgress: 0
    property bool positionRestorePending: false
    property int positionRestoreAttempts: 0

    readonly property int horizontalGutter: width < 900
                                                ? Theme.readerNarrowGutter
                                                : Theme.readerWideGutter
    readonly property int responsivePagePadding: width < 700
                                                     ? Theme.spaceLg
                                                     : pagePadding
    readonly property real maximumContentY: Math.max(0,
                                                      textFlickable.contentHeight
                                                      - textFlickable.height)
    readonly property real readingProgress: documentText.length === 0
                                                ? 0
                                                : maximumContentY <= 0
                                                  ? 1
                                                  : Math.max(0,
                                                             Math.min(1,
                                                                      textFlickable.contentY
                                                                      / maximumContentY))

    function scrollToStart() {
        textFlickable.contentY = 0
    }

    function restorePosition(progress) {
        root.pendingProgress = Math.max(0, Math.min(1, progress))
        root.positionRestorePending = true
        root.positionRestoreAttempts = 0
        positionRestoreTimer.restart()
    }

    function applyPendingPosition() {
        if (!root.positionRestorePending) {
            return
        }
        if (root.documentText.length > 0
                && (readerText.contentHeight <= 0
                    || (root.maximumContentY <= 0 && root.positionRestoreAttempts < 4))) {
            root.positionRestoreAttempts += 1
            positionRestoreTimer.restart()
            return
        }

        textFlickable.contentY = root.maximumContentY * root.pendingProgress
        root.positionRestorePending = false
    }

    function applyTextFormatting() {
        const progress = root.positionRestorePending
                         ? root.pendingProgress
                         : root.readingProgress
        root.documentFormatter.applyLineHeight(readerText.textDocument, root.lineHeight)
        root.restorePosition(progress)
    }

    onDocumentTextChanged: {
        root.positionRestorePending = false
        Qt.callLater(root.scrollToStart)
        Qt.callLater(root.applyTextFormatting)
    }
    onLineHeightChanged: Qt.callLater(root.applyTextFormatting)
    onMaximumContentYChanged: {
        if (root.positionRestorePending) {
            positionRestoreTimer.restart()
        }
    }
    Component.onCompleted: Qt.callLater(root.applyTextFormatting)

    Timer {
        id: positionRestoreTimer

        interval: 50
        onTriggered: root.applyPendingPosition()
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowColor
    }

    Flickable {
        id: textFlickable

        anchors.fill: parent
        clip: true
        contentWidth: width
        contentHeight: readerContainer.height
        flickableDirection: Flickable.VerticalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: LeaflitScrollBar {
        }

        Item {
            id: readerContainer

            width: textFlickable.width
            height: readerPage.height + root.horizontalGutter * 2

            LeaflitSurface {
                id: readerPage

                anchors.top: parent.top
                anchors.topMargin: root.horizontalGutter
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(0, Math.min(root.preferredPageWidth,
                                            parent.width - root.horizontalGutter * 2))
                height: Math.max(textFlickable.height - root.horizontalGutter * 2,
                                 readerText.contentHeight + root.responsivePagePadding * 2)
                fillColor: Theme.readingColor
                radius: Theme.radiusSm

                TextEdit {
                    id: readerText

                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.margins: root.responsivePagePadding
                    height: contentHeight
                    readOnly: true
                    selectByMouse: true
                    persistentSelection: true
                    text: root.documentText
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    color: Theme.textColor
                    selectionColor: Theme.accentColor
                    selectedTextColor: Theme.onAccentColor
                    font.family: Theme.readingFontFamily
                    font.pixelSize: root.fontSize
                    font.letterSpacing: 0
                    font.kerning: true
                    renderType: Text.NativeRendering
                    cursorVisible: false
                }
            }
        }
    }
}
