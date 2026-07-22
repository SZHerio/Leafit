import QtQuick
import QtQuick.Controls

Item {
    id: root

    required property string documentText
    required property var documentFormatter
    required property var searchController
    property string displayText: documentText
    property bool richText: false
    property string readingMode: "scroll"
    property string fontFamily: Theme.readingFontFamily
    property int fontSize: 18
    property real lineHeight: 1.5
    property int paragraphSpacing: 10
    property int firstLineIndent: 24
    property string textAlignment: "justify"
    property color linkColor: Theme.textColor
    property color pageColor: Theme.readingColor
    property int preferredPageWidth: Theme.readerPageMaxWidth
    property int pagePadding: Theme.readerPagePadding
    property real pendingProgress: 0
    property int pendingTextPosition: -1
    property bool positionRestorePending: false
    property int positionRestoreAttempts: 0
    property int paginationAttempts: 0
    property var pageOffsets: [0]
    property int currentPageIndex: 0
    property real pageWheelAccumulator: 0

    readonly property bool pagedMode: readingMode === "pages"
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
    readonly property real pageTextHeight: Math.max(1,
                                                     textFlickable.height
                                                     - root.horizontalGutter * 2
                                                     - root.responsivePagePadding * 2)
    readonly property real currentPageOffset: root.pageOffsets.length > 0
                                                   ? root.pageOffsets[Math.max(
                                                       0,
                                                       Math.min(root.currentPageIndex,
                                                                root.pageOffsets.length - 1))]
                                                   : 0
    readonly property real currentPageContentHeight: {
        const nextOffset = root.currentPageIndex + 1 < root.pageOffsets.length
                         ? root.pageOffsets[root.currentPageIndex + 1]
                         : readerText.contentHeight
        return Math.max(1, nextOffset - root.currentPageOffset)
    }
    readonly property int pageCount: root.pagedMode ? Math.max(1, root.pageOffsets.length) : 0
    readonly property int currentPage: root.pagedMode ? root.currentPageIndex : -1
    readonly property bool canPageBackward: root.pagedMode && root.currentPageIndex > 0
    readonly property bool canPageForward: root.pagedMode
                                                   && root.currentPageIndex < root.pageCount - 1
    readonly property int currentTextPosition: {
        if (root.documentText.length === 0 || readerText.length <= 0) {
            return 0
        }
        if (root.pagedMode) {
            return root.positionAtDocumentY(root.currentPageOffset)
        }
        const documentY = Math.max(0,
                                   textFlickable.contentY
                                   - readerPage.y
                                   - root.responsivePagePadding)
        return root.positionAtDocumentY(documentY)
    }
    readonly property real readingProgress: {
        if (root.documentText.length === 0) {
            return 0
        }
        if (root.pagedMode) {
            if (root.currentPageIndex >= root.pageCount - 1) {
                return 1
            }
            return Math.max(0,
                            Math.min(1,
                                     root.currentTextPosition
                                     / Math.max(1, readerText.length - 1)))
        }
        return root.maximumContentY <= 0
               ? 1
               : Math.max(0,
                          Math.min(1,
                                   textFlickable.contentY / root.maximumContentY))
    }

    signal semanticNavigationRequested(int textPosition)
    signal positionRestored

    function positionAtDocumentY(documentY) {
        return Math.max(0,
                        Math.min(readerText.length,
                                 readerText.positionAt(1,
                                                       Math.max(0, documentY))))
    }

    function pageForTextPosition(position) {
        if (root.pageOffsets.length <= 1) {
            return 0
        }
        const boundedPosition = Math.max(0, Math.min(readerText.length, position))
        const rectangle = readerText.positionToRectangle(boundedPosition)
        const targetY = Math.max(0, rectangle.y)
        let low = 0
        let high = root.pageOffsets.length - 1
        while (low < high) {
            const middle = Math.ceil((low + high) / 2)
            if (root.pageOffsets[middle] <= targetY + 0.5) {
                low = middle
            } else {
                high = middle - 1
            }
        }
        return low
    }

    function scrollToStart() {
        root.goToProgress(0)
    }

    function scrollToEnd() {
        root.goToProgress(1)
    }

    function scrollByPage(direction) {
        if (root.pagedMode) {
            root.goToPage(root.currentPageIndex + direction)
            return
        }
        const pageDistance = Math.max(120, textFlickable.height * 0.86)
        textFlickable.contentY = Math.max(
            0,
            Math.min(root.maximumContentY,
                     textFlickable.contentY + pageDistance * direction))
    }

    function goToPage(page) {
        if (!root.pagedMode || root.pageCount <= 0) {
            return
        }
        root.positionRestorePending = false
        root.currentPageIndex = Math.max(0, Math.min(root.pageCount - 1, page))
    }

    function goToProgress(progress) {
        root.positionRestorePending = false
        const boundedProgress = Math.max(0, Math.min(1, progress))
        if (root.pagedMode) {
            if (boundedProgress >= 1) {
                root.goToPage(root.pageCount - 1)
            } else {
                root.goToTextPosition(Math.round(boundedProgress
                                                 * Math.max(0, readerText.length - 1)))
            }
            return
        }
        textFlickable.contentY = root.maximumContentY * boundedProgress
    }

    function restorePosition(progress, textPosition) {
        root.pendingProgress = Math.max(0, Math.min(1, progress))
        root.pendingTextPosition = textPosition === undefined ? -1 : textPosition
        root.positionRestorePending = true
        root.positionRestoreAttempts = 0
        if (root.pagedMode) {
            textFlickable.contentY = 0
            root.paginationAttempts = 0
            paginationTimer.restart()
        } else {
            positionRestoreTimer.restart()
        }
    }

    function goToTextPosition(position) {
        if (position < 0 || root.documentText.length === 0) {
            return
        }
        root.positionRestorePending = false
        const boundedPosition = Math.min(position, readerText.length)
        if (root.pagedMode) {
            root.goToPage(root.pageForTextPosition(boundedPosition))
            return
        }
        const positionRect = readerText.positionToRectangle(boundedPosition)
        const targetY = readerPage.y + root.responsivePagePadding + positionRect.y
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

    function activateLink(link) {
        if (link.startsWith("#")) {
            const position = root.documentFormatter.anchorPosition(readerText.textDocument,
                                                                    link)
            if (position >= 0) {
                root.semanticNavigationRequested(position)
            }
            return
        }
        Qt.openUrlExternally(link)
    }

    function updateHighlightColors() {
        root.searchController.setHighlightColors(root.annotationHighlightColor,
                                                  root.searchMatchColor,
                                                  root.currentSearchMatchColor,
                                                  root.currentSearchTextColor)
    }

    function applyPendingPosition() {
        if (!root.positionRestorePending || root.pagedMode) {
            return
        }
        if (root.documentText.length > 0
                && (readerText.contentHeight <= 0
                    || (root.maximumContentY <= 0 && root.positionRestoreAttempts < 4))) {
            root.positionRestoreAttempts += 1
            positionRestoreTimer.restart()
            return
        }

        const targetPosition = root.pendingTextPosition
        const targetProgress = root.pendingProgress
        root.positionRestorePending = false
        if (targetPosition >= 0) {
            root.goToTextPosition(targetPosition)
        } else {
            textFlickable.contentY = root.maximumContentY * targetProgress
        }
        root.positionRestored()
    }

    function rebuildPagination() {
        if (!root.pagedMode) {
            root.pageOffsets = [0]
            root.currentPageIndex = 0
            return
        }
        if (readerText.width <= 0 || readerText.contentHeight <= 0
                || root.pageTextHeight <= 1) {
            if (root.paginationAttempts < 6) {
                root.paginationAttempts += 1
                paginationTimer.restart()
            }
            return
        }

        const offsets = [0]
        let offset = 0
        const maximumPages = 50000
        while (offset + root.pageTextHeight < readerText.contentHeight - 0.5
               && offsets.length < maximumPages) {
            const targetY = offset + root.pageTextHeight
            const position = readerText.positionAt(1, targetY)
            const rectangle = readerText.positionToRectangle(position)
            let nextOffset = rectangle.y
            if (!Number.isFinite(nextOffset)
                    || nextOffset <= offset + 1
                    || nextOffset > targetY) {
                nextOffset = targetY
            }
            offsets.push(nextOffset)
            offset = nextOffset
        }
        root.pageOffsets = offsets
        root.paginationAttempts = 0

        if (root.positionRestorePending) {
            const targetPosition = root.pendingTextPosition
            const targetProgress = root.pendingProgress
            root.positionRestorePending = false
            if (targetPosition >= 0) {
                root.goToTextPosition(targetPosition)
            } else {
                root.goToProgress(targetProgress)
            }
            root.positionRestored()
        } else {
            root.currentPageIndex = Math.max(0,
                                             Math.min(root.currentPageIndex,
                                                      offsets.length - 1))
        }
    }

    function applyTextFormatting() {
        const progress = root.positionRestorePending
                         ? root.pendingProgress
                         : root.readingProgress
        const textPosition = root.positionRestorePending
                             ? root.pendingTextPosition
                             : root.currentTextPosition
        root.documentFormatter.applyTypography(readerText.textDocument,
                                                root.lineHeight,
                                                root.paragraphSpacing,
                                                root.firstLineIndent,
                                                root.textAlignment,
                                                root.linkColor,
                                                root.pageColor)
        root.documentFormatter.fitImages(readerText.textDocument,
                                          Math.max(1, readerText.width),
                                          root.pagedMode ? root.pageTextHeight * 0.82 : 0)
        root.restorePosition(progress, textPosition)
    }

    function handlePageWheel(event) {
        if (!root.pagedMode || event.modifiers !== Qt.NoModifier) {
            return
        }
        const delta = event.pixelDelta.y !== 0
                    ? event.pixelDelta.y / 80
                    : event.angleDelta.y / 120
        if (delta === 0) {
            return
        }
        if (root.pageWheelAccumulator * delta < 0) {
            root.pageWheelAccumulator = 0
        }
        root.pageWheelAccumulator += delta
        if (Math.abs(root.pageWheelAccumulator) >= 1) {
            root.scrollByPage(root.pageWheelAccumulator < 0 ? 1 : -1)
            root.pageWheelAccumulator = 0
        }
        event.accepted = true
    }

    onDocumentTextChanged: {
        root.positionRestorePending = false
        root.pageOffsets = [0]
        root.currentPageIndex = 0
        if (root.searchController) {
            root.searchController.documentText = root.documentText
        }
        Qt.callLater(root.scrollToStart)
        Qt.callLater(root.applyTextFormatting)
    }
    onDisplayTextChanged: Qt.callLater(root.applyTextFormatting)
    onRichTextChanged: Qt.callLater(root.applyTextFormatting)
    onReadingModeChanged: Qt.callLater(root.applyTextFormatting)
    onLineHeightChanged: Qt.callLater(root.applyTextFormatting)
    onParagraphSpacingChanged: Qt.callLater(root.applyTextFormatting)
    onFirstLineIndentChanged: Qt.callLater(root.applyTextFormatting)
    onTextAlignmentChanged: Qt.callLater(root.applyTextFormatting)
    onLinkColorChanged: Qt.callLater(root.applyTextFormatting)
    onPageColorChanged: Qt.callLater(root.applyTextFormatting)
    onFontFamilyChanged: root.restorePosition(root.readingProgress,
                                              root.currentTextPosition)
    onFontSizeChanged: root.restorePosition(root.readingProgress,
                                            root.currentTextPosition)
    onPreferredPageWidthChanged: root.restorePosition(root.readingProgress,
                                                       root.currentTextPosition)
    onMaximumContentYChanged: {
        if (root.positionRestorePending && !root.pagedMode) {
            positionRestoreTimer.restart()
        }
    }
    onPageTextHeightChanged: {
        if (root.pagedMode) {
            root.restorePosition(root.readingProgress, root.currentTextPosition)
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
                root.semanticNavigationRequested(root.searchController.currentStart)
            }
        }
    }

    Timer {
        id: positionRestoreTimer

        interval: 50
        onTriggered: root.applyPendingPosition()
    }

    Timer {
        id: paginationTimer

        interval: 40
        onTriggered: root.rebuildPagination()
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
        contentHeight: root.pagedMode ? height : readerContainer.height
        flickableDirection: Flickable.VerticalFlick
        interactive: !root.pagedMode
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: SZHScrollBar {
            policy: root.pagedMode ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        }

        Item {
            id: readerContainer

            width: textFlickable.width
            height: root.pagedMode
                    ? textFlickable.height
                    : readerPage.height + root.horizontalGutter * 2

            SZHSurface {
                id: readerPage

                anchors.top: parent.top
                anchors.topMargin: root.horizontalGutter
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.max(0, Math.min(root.preferredPageWidth,
                                            parent.width - root.horizontalGutter * 2))
                height: root.pagedMode
                        ? Math.max(0, textFlickable.height - root.horizontalGutter * 2)
                        : textViewport.height + root.responsivePagePadding * 2
                fillColor: Theme.readingColor
                radius: Theme.radiusSm

                Item {
                    id: textViewport

                    x: root.responsivePagePadding
                    y: root.responsivePagePadding
                    width: Math.max(0, parent.width - root.responsivePagePadding * 2)
                    height: root.pagedMode
                            ? Math.min(root.pageTextHeight,
                                       root.currentPageContentHeight)
                            : readerText.contentHeight
                    clip: root.pagedMode

                    TextEdit {
                        id: readerText

                        x: 0
                        y: root.pagedMode ? -root.currentPageOffset : 0
                        width: parent.width
                        height: contentHeight
                        readOnly: true
                        selectByMouse: true
                        persistentSelection: true
                        text: root.richText ? root.displayText : root.documentText
                        textFormat: root.richText ? TextEdit.RichText : TextEdit.PlainText
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
                        onLinkActivated: link => root.activateLink(link)
                        onWidthChanged: Qt.callLater(root.applyTextFormatting)
                        onContentHeightChanged: {
                            if (root.pagedMode) {
                                paginationTimer.restart()
                            }
                        }
                    }
                }
            }
        }
    }

    SZHIconButton {
        anchors.left: parent.left
        anchors.leftMargin: Theme.spaceSm
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        visible: root.pagedMode && root.width >= 620
        enabled: root.canPageBackward
        symbol: "\u2039"
        symbolPixelSize: Theme.displayFontSize
        toolTip: qsTr("Previous page")
        onClicked: root.scrollByPage(-1)
    }

    SZHIconButton {
        anchors.right: parent.right
        anchors.rightMargin: Theme.spaceSm
        anchors.verticalCenter: parent.verticalCenter
        z: 2
        visible: root.pagedMode && root.width >= 620
        enabled: root.canPageForward
        symbol: "\u203a"
        symbolPixelSize: Theme.displayFontSize
        toolTip: qsTr("Next page")
        onClicked: root.scrollByPage(1)
    }

    WheelHandler {
        enabled: root.pagedMode
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        acceptedModifiers: Qt.NoModifier
        orientation: Qt.Vertical
        target: null
        blocking: true
        onWheel: event => root.handlePageWheel(event)
    }
}
