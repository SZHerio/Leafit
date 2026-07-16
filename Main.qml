import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: root

    required property var readerController
    required property var localStateStore
    required property var libraryModel
    required property var readingDocumentFormatter
    required property var readingSearchController
    required property var readingAnnotationStore

    property bool showingLibrary: true

    width: 1120
    height: 760
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: showingLibrary
               ? qsTr("Library - SZHBooks")
               : readerController.title.length > 0
                 ? readerController.title + qsTr(" - SZHBooks")
                 : qsTr("SZHBooks")

    color: Theme.windowColor

    function openBook(fileUrl) {
        if (root.readerController.openFile(fileUrl)) {
            root.showingLibrary = false
        }
    }

    function showLibrary() {
        readerWorkspace.flushReadingState()
        root.localStateStore.sync()
        root.libraryModel.refresh()
        root.showingLibrary = true
    }

    function toggleColorTheme() {
        root.localStateStore.darkMode = !root.localStateStore.darkMode
    }

    onClosing: {
        readerWorkspace.flushReadingState()
        root.localStateStore.sync()
    }

    Binding {
        target: Theme
        property: "colorTheme"
        value: root.localStateStore.colorTheme
    }

    Shortcut {
        sequence: StandardKey.Open
        onActivated: openDialog.open()
    }

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: StandardKey.Find
        onActivated: appHeader.openSearch()
    }

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: StandardKey.ZoomIn
        onActivated: readerWorkspace.increaseScale()
    }

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: StandardKey.ZoomOut
        onActivated: readerWorkspace.decreaseScale()
    }

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: "Ctrl+B"
        onActivated: readerWorkspace.toggleCurrentBookmark()
    }

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: "Ctrl+Shift+H"
        onActivated: appHeader.openAnnotations("highlights")
    }

    Shortcut {
        sequence: "Ctrl+Shift+D"
        onActivated: root.toggleColorTheme()
    }

    FileDialog {
        id: openDialog

        title: qsTr("Open book")
        nameFilters: [
            qsTr("Supported books (*.txt *.fb2 *.epub *.pdf *.html *.htm *.md *.markdown *.docx)"),
            qsTr("Text files (*.txt *.md *.markdown *.html *.htm)"),
            qsTr("Electronic books (*.fb2 *.epub)"),
            qsTr("PDF files (*.pdf)"),
            qsTr("Word documents (*.docx)")
        ]

        onAccepted: root.openBook(selectedFile)
    }

    header: AppHeader {
        id: appHeader
        objectName: "appHeader"

        readerController: root.readerController
        readerWorkspace: readerWorkspace
        settingsStore: root.localStateStore
        darkMode: root.localStateStore.darkMode
        showingLibrary: root.showingLibrary
        onOpenRequested: openDialog.open()
        onLibraryRequested: root.showLibrary()
        onDarkModeToggled: root.localStateStore.darkMode = darkMode
    }

    footer: ReaderStatusBar {
        readerController: root.readerController
        readerWorkspace: readerWorkspace
        visible: !root.showingLibrary
    }

    ReaderWorkspace {
        id: readerWorkspace
        objectName: "readerWorkspace"

        anchors.fill: parent
        visible: !root.showingLibrary
        opacity: root.showingLibrary ? 0 : 1
        readerController: root.readerController
        settingsStore: root.localStateStore
        documentFormatter: root.readingDocumentFormatter
        searchController: root.readingSearchController
        annotationStore: root.readingAnnotationStore
        onOpenRequested: openDialog.open()

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    LibraryView {
        anchors.fill: parent
        visible: root.showingLibrary
        opacity: root.showingLibrary ? 1 : 0
        libraryModel: root.libraryModel
        onAddRequested: openDialog.open()
        onOpenRequested: sourceUrl => root.openBook(sourceUrl)

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    SZHNotification {
        z: 100
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spaceMd
        anchors.rightMargin: Theme.spaceMd
        width: Math.min(480, Math.max(320, root.width - Theme.spaceXl))
        shown: root.readerController.errorMessage.length > 0
        heading: qsTr("Could not open book")
        message: root.readerController.errorMessage
        actionText: qsTr("Choose another")
        onActionRequested: openDialog.open()
        onDismissed: root.readerController.clearError()
    }
}
