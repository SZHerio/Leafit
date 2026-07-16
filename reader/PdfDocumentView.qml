import QtQuick
import QtQuick.Controls
import QtQuick.Pdf
import QtQuick.Layouts

Item {
    id: root

    required property url source

    property int pendingPage: 0
    property real pendingScale: 1.0
    property bool stateRestorePending: false
    property string searchQuery: ""
    property bool passwordAttempted: false
    property bool passwordRequired: false

    readonly property real renderScale: pdfViewer.renderScale
    readonly property bool canZoomOut: renderScale > 0.4
    readonly property bool canZoomIn: renderScale < 3.0
    readonly property int currentPage: pdfDocument.pageCount > 0
                                           ? Math.max(0, pdfViewer.currentPage)
                                           : -1
    readonly property int pageCount: pdfDocument.pageCount
    readonly property string selectedText: pdfViewer.selectedText
    readonly property int searchResultCount: pdfViewer.searchModel.count
    readonly property int currentSearchIndex: searchResultCount > 0
                                                       ? pdfViewer.searchModel.currentResult
                                                       : -1
    readonly property real readingProgress: pageCount > 0
                                                ? Math.max(0,
                                                           Math.min(1,
                                                                    (currentPage + 1)
                                                                    / pageCount))
                                                : 0
    readonly property bool documentHasError: pdfDocument.status === PdfDocument.Error
    readonly property string documentError: pdfDocument.error.length > 0
                                                       ? pdfDocument.error
                                                       : qsTr("The PDF could not be opened.")

    function goToPage(page) {
        if (root.pageCount <= 0) {
            return
        }
        pdfViewer.goToPage(Math.max(0, Math.min(root.pageCount - 1, page)))
    }

    function goToStart() {
        root.goToPage(0)
    }

    function goToEnd() {
        root.goToPage(root.pageCount - 1)
    }

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

    function clearSelection() {
        pdfViewer.selectedText = ""
    }

    function copySelection() {
        if (pdfViewer.selectedText.length > 0) {
            pdfViewer.copySelectionToClipboard()
        }
    }

    function nextSearchResult() {
        if (root.searchResultCount <= 0) {
            return
        }
        const current = Math.max(-1, pdfViewer.searchModel.currentResult)
        pdfViewer.searchModel.currentResult = (current + 1) % root.searchResultCount
    }

    function previousSearchResult() {
        if (root.searchResultCount <= 0) {
            return
        }
        const current = pdfViewer.searchModel.currentResult < 0
                        ? 0
                        : pdfViewer.searchModel.currentResult
        pdfViewer.searchModel.currentResult = (current - 1 + root.searchResultCount)
                                              % root.searchResultCount
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

        root.goToPage(root.pendingPage)
        root.stateRestorePending = false
    }

    onSourceChanged: {
        root.stateRestorePending = false
        root.passwordAttempted = false
        root.passwordRequired = false
        passwordDialog.close()
        pdfDocument.password = ""
        pdfViewer.renderScale = 1.0
    }
    onSearchQueryChanged: {
        pdfViewer.searchString = root.searchQuery
        if (root.searchQuery.trim().length === 0) {
            pdfViewer.searchModel.currentResult = -1
        }
    }
    onSearchResultCountChanged: {
        if (root.searchResultCount <= 0) {
            pdfViewer.searchModel.currentResult = -1
        } else if (root.searchQuery.trim().length > 0
                   && (pdfViewer.searchModel.currentResult < 0
                       || pdfViewer.searchModel.currentResult >= root.searchResultCount)) {
            pdfViewer.searchModel.currentResult = 0
        }
    }
    onPageCountChanged: {
        if (root.stateRestorePending) {
            Qt.callLater(root.applyPendingState)
        }
    }

    PdfDocument {
        id: pdfDocument

        source: root.source
        onPasswordRequired: {
            root.passwordRequired = true
            passwordDialog.requestPassword(root.passwordAttempted)
        }
        onStatusChanged: status => {
            if (status === PdfDocument.Ready) {
                root.passwordAttempted = false
                root.passwordRequired = false
                passwordDialog.close()
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowColor
    }

    SZHSurface {
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

    SZHSurface {
        anchors.centerIn: parent
        width: Math.max(0, Math.min(420, parent.width - Theme.spaceXl))
        height: errorContent.implicitHeight + Theme.spaceXl * 2
        fillColor: Theme.surfaceColor
        radius: Theme.radiusLg
        visible: root.documentHasError && !passwordDialog.visible
        z: 2

        ColumnLayout {
            id: errorContent

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.margins: Theme.spaceLg
            spacing: Theme.spaceSm

            Label {
                Layout.fillWidth: true
                text: root.passwordRequired
                      ? qsTr("Password required")
                      : qsTr("Could not open PDF")
                color: Theme.textColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.titleFontSize
                font.weight: Font.DemiBold
            }

            Label {
                Layout.fillWidth: true
                text: root.passwordRequired
                      ? qsTr("This document is protected. Enter its password to continue.")
                      : root.documentError
                color: Theme.mutedTextColor
                font.family: Theme.uiFontFamily
                font.pixelSize: Theme.bodyFontSize
                wrapMode: Text.Wrap
            }

            SZHButton {
                visible: root.passwordRequired
                text: qsTr("Enter password")
                onClicked: passwordDialog.requestPassword(root.passwordAttempted)
            }
        }
    }

    PdfPasswordDialog {
        id: passwordDialog

        onPasswordSubmitted: password => {
            root.passwordAttempted = true
            pdfDocument.password = password
        }
    }
}
