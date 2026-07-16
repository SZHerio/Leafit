import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property string documentText
    required property var documentFormatter
    required property var searchController
    property string fontFamily: Theme.readingFontFamily
    property int fontSize: 18
    property real lineHeight: 1.5
    property int paragraphSpacing: 10
    property int firstLineIndent: 24
    property string textAlignment: "justify"
    property int preferredPageWidth: Theme.readerPageMaxWidth
    property int pagePadding: Theme.readerPagePadding
    property real pendingProgress: 0
    property bool positionRestorePending: false
    property int positionRestoreAttempts: 0

    readonly property string selectedText: readerText.selectedText
    readonly property int selectionStart: readerText.selectionStart
    readonly property int selectionEnd: readerText.selectionEnd
    readonly property color annotationHighlightColor: Theme.annotationHighlightColor
    readonly property color searchMatchColor: Theme.searchMatchColor
    readonly property color currentSearchMatchColor: Theme.currentSearchMatchColor
    readonly property color currentSearchTextColor: Theme.currentSearchTextColor

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
        root.goToProgress(0)
    }

    function scrollToEnd() {
        root.goToProgress(1)
    }

    function scrollByPage(direction) {
        const pageDistance = Math.max(120, textFlickable.height * 0.86)
        textFlickable.contentY = Math.max(
            0,
            Math.min(root.maximumContentY,
                     textFlickable.contentY + pageDistance * direction))
    }

    function goToProgress(progress) {
        root.positionRestorePending = false
        textFlickable.contentY = root.maximumContentY * Math.max(0, Math.min(1, progress))
    }

    function restorePosition(progress) {
        root.pendingProgress = Math.max(0, Math.min(1, progress))
        root.positionRestorePending = true
        root.positionRestoreAttempts = 0
        positionRestoreTimer.restart()
    }

    function goToTextPosition(position) {
        if (position < 0 || root.documentText.length === 0) {
            return
        }
        const positionRect = readerText.positionToRectangle(
            Math.min(position, root.documentText.length))
        const targetY = readerPage.y + readerText.y + positionRect.y
                        - textFlickable.height * 0.32
        textFlickable.contentY = Math.max(0, Math.min(root.maximumContentY, targetY))
    }

    function clearSelection() {
        readerText.deselect()
    }

    function copySelection() {
        if (readerText.selectedText.length > 0) {
            readerText.copy()
        }
    }

    function updateHighlightColors() {
        root.searchController.setHighlightColors(root.annotationHighlightColor,
                                                 root.searchMatchColor,
                                                 root.currentSearchMatchColor,
                                                 root.currentSearchTextColor)
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
        root.documentFormatter.applyTypography(readerText.textDocument,
                                               root.lineHeight,
                                               root.paragraphSpacing,
                                               root.firstLineIndent,
                                               root.textAlignment)
        root.restorePosition(progress)
    }

    onDocumentTextChanged: {
        root.positionRestorePending = false
        root.searchController.documentText = root.documentText
        Qt.callLater(root.scrollToStart)
        Qt.callLater(root.applyTextFormatting)
    }
    onLineHeightChanged: Qt.callLater(root.applyTextFormatting)
    onParagraphSpacingChanged: Qt.callLater(root.applyTextFormatting)
    onFirstLineIndentChanged: Qt.callLater(root.applyTextFormatting)
    onTextAlignmentChanged: Qt.callLater(root.applyTextFormatting)
    onFontFamilyChanged: root.restorePosition(root.readingProgress)
    onFontSizeChanged: root.restorePosition(root.readingProgress)
    onPreferredPageWidthChanged: root.restorePosition(root.readingProgress)
    onMaximumContentYChanged: {
        if (root.positionRestorePending) {
            positionRestoreTimer.restart()
        }
    }
    onAnnotationHighlightColorChanged: root.updateHighlightColors()
    onSearchMatchColorChanged: root.updateHighlightColors()
    onCurrentSearchMatchColorChanged: root.updateHighlightColors()
    onCurrentSearchTextColorChanged: root.updateHighlightColors()
    Component.onCompleted: {
        root.searchController.attachTextDocument(readerText.textDocument)
        root.searchController.documentText = root.documentText
        root.updateHighlightColors()
        Qt.callLater(root.applyTextFormatting)
    }

    Connections {
        target: root.searchController

        function onCurrentResultChanged() {
            if (root.visible && root.searchController.currentStart >= 0) {
                Qt.callLater(function() {
                    root.goToTextPosition(root.searchController.currentStart)
                })
            }
        }
    }

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
        ScrollBar.vertical: SZHScrollBar {
        }

        Item {
            id: readerContainer

            width: textFlickable.width
            height: readerPage.height + root.horizontalGutter * 2

            SZHSurface {
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
                    font.family: root.fontFamily
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
