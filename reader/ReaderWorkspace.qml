import QtQuick

Item {
    id: root

    required property var readerController
    required property var settingsStore
    required property var documentFormatter
    required property var searchController
    required property var annotationStore

    readonly property int textFontSize: settingsStore.textFontSize
    readonly property string textFontFamily: Theme.readingFontFamilyFor(settingsStore.readingFont)
    readonly property int preferredPageWidth: settingsStore.pageWidth
    readonly property real lineHeight: settingsStore.lineHeight
    readonly property int paragraphSpacing: settingsStore.paragraphSpacing
    readonly property int firstLineIndent: settingsStore.firstLineIndent
    readonly property string textAlignment: settingsStore.textAlignment
    readonly property url activeDocumentUrl: readerController.sourceUrl
    readonly property bool hasDocument: readerController.hasDocument
    readonly property bool showingText: readerController.textMode
    readonly property bool showingPdf: readerController.pdfMode
    readonly property real readingProgress: showingPdf
                                                ? pdfView.readingProgress
                                                : showingText
                                                  ? textView.readingProgress
                                                  : 0
    readonly property int currentPage: showingPdf ? pdfView.currentPage : -1
    readonly property int pageCount: showingPdf ? pdfView.pageCount : 0
    readonly property var chapters: readerController.chapters
    readonly property bool hasChapters: showingText && chapters.length > 0
    readonly property int currentChapterIndex: root.chapterIndexAt(root.readingProgress)
    readonly property string currentChapterTitle: currentChapterIndex >= 0
                                                           ? chapters[currentChapterIndex].title
                                                           : ""
    readonly property bool canGoBackward: showingPdf
                                               ? currentPage > 0
                                               : showingText && readingProgress > 0
    readonly property bool canGoForward: showingPdf
                                              ? pageCount > 0 && currentPage < pageCount - 1
                                              : showingText && readingProgress < 1
    readonly property bool canDecreaseScale: hasDocument
                                                && (showingPdf
                                                    ? pdfView.canZoomOut
                                                    : textFontSize > 12)
    readonly property bool canIncreaseScale: hasDocument
                                                && (showingPdf
                                                    ? pdfView.canZoomIn
                                                    : textFontSize < 36)
    readonly property string scaleLabel: showingPdf
                                             ? Math.round(pdfView.renderScale * 100) + qsTr("%")
                                             : textFontSize + qsTr(" px")
    readonly property int searchResultCount: !hasDocument
                                                   ? 0
                                                   : showingPdf
                                                     ? pdfView.searchResultCount
                                                     : searchController.resultCount
    readonly property int currentSearchIndex: searchResultCount <= 0
                                                   ? -1
                                                   : showingPdf
                                                     ? pdfView.currentSearchIndex
                                                     : searchController.currentIndex
    readonly property string selectedText: showingPdf
                                               ? pdfView.selectedText
                                               : showingText
                                                 ? textView.selectedText
                                                 : ""
    readonly property bool canCreateHighlight: hasDocument
                                                   && selectedText.trim().length > 0
    readonly property bool currentLocationBookmarked: annotationStore.totalCount >= 0
                                                          && hasDocument
                                                          && annotationStore.hasBookmark(
                                                              readingProgress,
                                                              showingPdf ? currentPage : -1)
    readonly property int annotationCount: annotationStore.totalCount
    readonly property bool sidebarOverlay: width < 980
    readonly property string sidebarTab: readerSidebar.activeTab
    readonly property bool sidebarTextInputActive: sidebarOpen
                                                     && readerSidebar.noteEditorActive
    readonly property bool selectionActionsVisible: canCreateHighlight
                                                     && !(sidebarOpen
                                                          && sidebarTab === "notes")

    property bool restoringReadingState: false
    property real scaleWheelAccumulator: 0
    property string searchQuery: ""
    property bool sidebarOpen: false

    signal openRequested

    onSearchQueryChanged: {
        root.searchController.query = root.searchQuery
        pdfView.searchQuery = root.searchQuery
    }

    function configureDocumentTools() {
        root.searchQuery = ""
        root.annotationStore.documentUrl = root.activeDocumentUrl
        root.searchController.documentText = root.showingText
                                             ? root.readerController.text
                                             : ""
        root.updateAnnotationHighlights()
    }

    function updateAnnotationHighlights() {
        root.searchController.setAnnotationRanges(root.annotationStore.highlights)
    }

    function nextSearchResult() {
        if (root.showingPdf) {
            pdfView.nextSearchResult()
        } else if (root.showingText) {
            root.searchController.next()
        }
    }

    function previousSearchResult() {
        if (root.showingPdf) {
            pdfView.previousSearchResult()
        } else if (root.showingText) {
            root.searchController.previous()
        }
    }

    function toggleCurrentBookmark() {
        if (!root.hasDocument) {
            return
        }
        const page = root.showingPdf ? root.currentPage : -1
        const label = root.showingPdf
                      ? qsTr("Page %1").arg(root.currentPage + 1)
                      : root.currentChapterTitle.length > 0
                        ? root.currentChapterTitle
                        : qsTr("%1%").arg(Math.round(root.readingProgress * 100))
        root.annotationStore.toggleBookmark(root.readingProgress, page, label)
    }

    function addSelectionHighlight(note) {
        if (!root.canCreateHighlight) {
            return
        }

        let start = -1
        let length = 0
        let page = -1
        let progress = root.readingProgress
        if (root.showingText) {
            start = Math.min(textView.selectionStart, textView.selectionEnd)
            length = Math.abs(textView.selectionEnd - textView.selectionStart)
            progress = start / Math.max(1, root.readerController.text.length)
        } else if (root.showingPdf) {
            page = root.currentPage
        }

        root.annotationStore.addHighlight(start,
                                          length,
                                          root.selectedText,
                                          note,
                                          progress,
                                          page)
        root.clearSelection()
    }

    function clearSelection() {
        if (root.showingText) {
            textView.clearSelection()
        } else if (root.showingPdf) {
            pdfView.clearSelection()
        }
    }

    function copySelection() {
        if (root.showingText) {
            textView.copySelection()
        } else if (root.showingPdf) {
            pdfView.copySelection()
        }
    }

    function openSidebar(tab) {
        if (!root.hasDocument) {
            return
        }
        readerSidebar.activeTab = tab === "bookmarks" || tab === "notes"
                                  ? tab : "contents"
        root.sidebarOpen = true
    }

    function toggleSidebar(tab) {
        const normalizedTab = tab === "bookmarks" || tab === "notes"
                              ? tab : "contents"
        if (root.sidebarOpen && readerSidebar.activeTab === normalizedTab) {
            root.sidebarOpen = false
        } else {
            root.openSidebar(normalizedTab)
        }
    }

    function closeSidebar() {
        root.sidebarOpen = false
    }

    function beginSelectionNote() {
        if (!root.canCreateHighlight) {
            return
        }
        root.openSidebar("notes")
        readerSidebar.focusSelectionNote()
    }

    function goToAnnotation(annotation) {
        if (root.showingPdf && annotation.page >= 0) {
            root.goToPage(annotation.page)
        } else if (root.showingText && annotation.start >= 0) {
            textView.goToTextPosition(annotation.start)
        } else if (root.showingText) {
            root.goToProgress(annotation.progress)
        }
    }

    function decreaseScale() {
        if (root.showingPdf) {
            pdfView.zoomOut()
        } else if (root.showingText) {
            root.settingsStore.textFontSize = Math.max(12, root.textFontSize - 1)
        }
    }

    function increaseScale() {
        if (root.showingPdf) {
            pdfView.zoomIn()
        } else if (root.showingText) {
            root.settingsStore.textFontSize = Math.min(36, root.textFontSize + 1)
        }
    }

    function fitPdfToWidth() {
        if (root.showingPdf) {
            pdfView.fitToWidth()
        }
    }

    function goToStart() {
        if (root.showingPdf) {
            pdfView.goToStart()
        } else if (root.showingText) {
            textView.scrollToStart()
        }
    }

    function goToEnd() {
        if (root.showingPdf) {
            pdfView.goToEnd()
        } else if (root.showingText) {
            textView.scrollToEnd()
        }
    }

    function goToProgress(progress) {
        if (root.showingText) {
            textView.goToProgress(progress)
        } else if (root.showingPdf && root.pageCount > 0) {
            root.goToPage(Math.round(Math.max(0, Math.min(1, progress))
                                     * (root.pageCount - 1)))
        }
    }

    function goToPage(page) {
        if (root.showingPdf) {
            pdfView.goToPage(page)
        }
    }

    function previousPage() {
        root.goToPage(root.currentPage - 1)
    }

    function nextPage() {
        root.goToPage(root.currentPage + 1)
    }

    function pageBackward() {
        if (root.showingPdf) {
            root.previousPage()
        } else if (root.showingText) {
            textView.scrollByPage(-1)
        }
    }

    function pageForward() {
        if (root.showingPdf) {
            root.nextPage()
        } else if (root.showingText) {
            textView.scrollByPage(1)
        }
    }

    function chapterIndexAt(progress) {
        if (!root.hasChapters) {
            return -1
        }

        let currentIndex = 0
        for (let index = 0; index < root.chapters.length; ++index) {
            if (root.chapters[index].progress > progress + 0.0001) {
                break
            }
            currentIndex = index
        }
        return currentIndex
    }

    function goToChapter(index) {
        if (index >= 0 && index < root.chapters.length) {
            root.goToProgress(root.chapters[index].progress)
        }
    }

    function handleScaleWheel(event) {
        const normalizedDelta = event.pixelDelta.y !== 0
                                ? event.pixelDelta.y / 80
                                : event.angleDelta.y / 120
        if (normalizedDelta === 0) {
            return
        }

        if (root.scaleWheelAccumulator * normalizedDelta < 0) {
            root.scaleWheelAccumulator = 0
        }
        root.scaleWheelAccumulator += normalizedDelta

        const steps = root.scaleWheelAccumulator > 0
                      ? Math.floor(root.scaleWheelAccumulator)
                      : Math.ceil(root.scaleWheelAccumulator)
        for (let step = 0; step < Math.abs(steps); ++step) {
            if (steps > 0) {
                root.increaseScale()
            } else {
                root.decreaseScale()
            }
        }

        root.scaleWheelAccumulator -= steps
        event.accepted = true
    }

    function scheduleReadingStateSave() {
        if (!root.restoringReadingState && root.hasDocument) {
            savePositionTimer.restart()
        }
    }

    function saveReadingState() {
        if (!root.hasDocument || root.activeDocumentUrl.toString().length === 0) {
            return
        }

        if (root.showingPdf) {
            if (pdfView.pageCount > 0) {
                root.settingsStore.savePdfPosition(root.activeDocumentUrl,
                                                   pdfView.currentPage,
                                                   pdfView.renderScale,
                                                   pdfView.readingProgress)
            }
        } else if (root.showingText) {
            root.settingsStore.saveTextPosition(root.activeDocumentUrl,
                                                textView.readingProgress)
        }
    }

    function flushReadingState() {
        savePositionTimer.stop()
        root.saveReadingState()
    }

    function restoreReadingState() {
        savePositionTimer.stop()
        if (!root.hasDocument || root.activeDocumentUrl.toString().length === 0) {
            return
        }

        root.restoringReadingState = true
        if (root.showingPdf) {
            pdfView.restoreState(root.settingsStore.pdfPage(root.activeDocumentUrl),
                                 root.settingsStore.pdfScale(root.activeDocumentUrl))
        } else if (root.showingText) {
            textView.restorePosition(root.settingsStore.textPosition(root.activeDocumentUrl))
        }
        restoreGuardTimer.restart()
    }

    Component.onCompleted: {
        root.configureDocumentTools()
        Qt.callLater(root.restoreReadingState)
    }

    Connections {
        target: root.readerController

        function onDocumentOpening() {
            root.flushReadingState()
            root.searchQuery = ""
            root.sidebarOpen = false
        }

        function onDocumentOpened() {
            Qt.callLater(function() {
                root.configureDocumentTools()
                root.restoreReadingState()
            })
        }
    }

    Connections {
        target: root.annotationStore

        function onAnnotationsChanged() {
            root.updateAnnotationHighlights()
        }
    }

    Timer {
        id: savePositionTimer

        interval: 600
        onTriggered: root.saveReadingState()
    }

    Timer {
        id: restoreGuardTimer

        interval: 100
        onTriggered: root.restoringReadingState = false
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowColor

        Behavior on color {
            ColorAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    Item {
        id: readerViewport

        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.selectionActionsVisible
                              ? selectionActions.height + Theme.spaceLg * 2 : 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.rightMargin: root.sidebarOpen && !root.sidebarOverlay
                             ? readerSidebar.width : 0

        Behavior on anchors.rightMargin {
            NumberAnimation {
                duration: Theme.motionNormal
                easing.type: Easing.OutCubic
            }
        }

        Behavior on anchors.bottomMargin {
            NumberAnimation {
                duration: Theme.motionFast
                easing.type: Easing.OutCubic
            }
        }

        EmptyReaderView {
            anchors.fill: parent
            visible: !root.hasDocument
            errorMessage: root.readerController.errorMessage
            onOpenRequested: root.openRequested()
        }

        TextDocumentView {
            id: textView

            anchors.fill: parent
            visible: root.showingText
            documentText: root.readerController.text
            displayText: root.readerController.displayText
            richText: root.readerController.richText
            documentFormatter: root.documentFormatter
            searchController: root.searchController
            fontFamily: root.textFontFamily
            fontSize: root.textFontSize
            lineHeight: root.lineHeight
            paragraphSpacing: root.paragraphSpacing
            firstLineIndent: root.firstLineIndent
            textAlignment: root.textAlignment
            preferredPageWidth: root.preferredPageWidth
            onReadingProgressChanged: root.scheduleReadingStateSave()
        }

        PdfDocumentView {
            id: pdfView

            anchors.fill: parent
            visible: root.showingPdf
            source: root.readerController.pdfSource
            onReadingProgressChanged: root.scheduleReadingStateSave()
            onRenderScaleChanged: root.scheduleReadingStateSave()
        }

    }

    SelectionActionBar {
        id: selectionActions

        anchors.horizontalCenter: readerViewport.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.spaceLg
        z: 3
        visible: root.selectionActionsVisible
        opacity: visible ? 1 : 0
        onCopyRequested: root.copySelection()
        onHighlightRequested: root.addSelectionHighlight("")
        onNoteRequested: root.beginSelectionNote()
        onCloseRequested: root.clearSelection()

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionFast
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 4
        visible: root.sidebarOpen && root.sidebarOverlay
        color: Theme.darkMode ? "#99000000" : "#66000000"

        MouseArea {
            anchors.fill: parent
            onClicked: root.closeSidebar()
        }
    }

    ReaderSidebar {
        id: readerSidebar

        z: 5
        y: 0
        x: root.sidebarOpen ? root.width - width : root.width
        width: Math.min(360, Math.max(320, root.width * 0.88))
        height: root.height
        readerWorkspace: root
        annotationStore: root.annotationStore
        onCloseRequested: root.closeSidebar()

        Behavior on x {
            NumberAnimation {
                duration: Theme.motionNormal
                easing.type: Easing.OutCubic
            }
        }
    }

    Item {
        anchors.fill: parent
        z: 1

        WheelHandler {
            enabled: root.hasDocument
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            acceptedModifiers: Qt.ControlModifier
            orientation: Qt.Vertical
            target: null
            blocking: true
            onWheel: event => root.handleScaleWheel(event)
        }
    }
}
