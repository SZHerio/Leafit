import QtQuick
import QtQuick.Pdf

Item {
    id: root

    required property url source

    property int pendingPage: 0
    property real pendingScale: 1.0
    property bool stateRestorePending: false

    readonly property real renderScale: pdfViewer.renderScale
    readonly property bool canZoomOut: renderScale > 0.4
    readonly property bool canZoomIn: renderScale < 3.0
    readonly property int currentPage: pdfDocument.pageCount > 0
                                           ? Math.max(0, pdfViewer.currentPage)
                                           : -1
    readonly property int pageCount: pdfDocument.pageCount
    readonly property real readingProgress: pageCount > 0
                                                ? Math.max(0,
                                                           Math.min(1,
                                                                    (currentPage + 1)
                                                                    / pageCount))
                                                : 0

    function zoomOut() {
        pdfViewer.renderScale = Math.max(0.4, pdfViewer.renderScale - 0.1)
    }

    function zoomIn() {
        pdfViewer.renderScale = Math.min(3.0, pdfViewer.renderScale + 0.1)
    }

    function fitToWidth() {
        pdfViewer.scaleToWidth(Math.max(0, pdfHost.width - Theme.spaceXl),
                               pdfHost.height)
    }

    function restoreState(page, scale) {
        root.pendingPage = Math.max(0, page)
        root.pendingScale = Math.max(0.4, Math.min(3.0, scale))
        root.stateRestorePending = true
        Qt.callLater(root.applyPendingState)
    }

    function applyPendingState() {
        if (!root.stateRestorePending) {
            return
        }

        pdfViewer.renderScale = root.pendingScale
        if (root.pageCount <= 0) {
            return
        }

        pdfViewer.goToPage(Math.min(root.pendingPage, root.pageCount - 1))
        root.stateRestorePending = false
    }

    onSourceChanged: {
        root.stateRestorePending = false
        pdfViewer.renderScale = 1.0
    }
    onPageCountChanged: {
        if (root.stateRestorePending) {
            Qt.callLater(root.applyPendingState)
        }
    }

    PdfDocument {
        id: pdfDocument

        source: root.source
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowColor
    }

    LeaflitSurface {
        id: pdfHost

        anchors.fill: parent
        anchors.margins: root.width < 900 ? Theme.spaceMd : Theme.spaceLg
        fillColor: Theme.surfaceColor
        clip: true

        PdfMultiPageView {
            id: pdfViewer

            anchors.fill: parent
            anchors.margins: Theme.spaceSm
            document: pdfDocument
        }
    }
}
