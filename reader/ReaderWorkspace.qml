import QtQuick

Item {
    id: root

    required property var readerController
    required property var settingsStore
    required property var documentFormatter

    readonly property int textFontSize: settingsStore.textFontSize
    readonly property string textFontFamily: Theme.readingFontFamilyFor(settingsStore.readingFont)
    readonly property int preferredPageWidth: settingsStore.pageWidth
    readonly property real lineHeight: settingsStore.lineHeight
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

    property bool restoringReadingState: false
    property real scaleWheelAccumulator: 0

    signal openRequested

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

    Component.onCompleted: Qt.callLater(root.restoreReadingState)

    Connections {
        target: root.readerController

        function onDocumentOpening() {
            root.flushReadingState()
        }

        function onDocumentOpened() {
            Qt.callLater(root.restoreReadingState)
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
        documentFormatter: root.documentFormatter
        fontFamily: root.textFontFamily
        fontSize: root.textFontSize
        lineHeight: root.lineHeight
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
