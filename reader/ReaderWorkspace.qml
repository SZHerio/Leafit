import QtQuick

Item {
    id: root

    required property var readerController
    required property var settingsStore
    required property var documentFormatter

    readonly property int textFontSize: settingsStore.textFontSize
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
                                                   pdfView.renderScale)
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
}
