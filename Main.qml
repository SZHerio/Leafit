import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: root

    required property var readerController
    required property var localStateStore
    required property var libraryModel
    required property var readingDocumentFormatter

    property bool showingLibrary: true

    width: 1120
    height: 760
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: showingLibrary
               ? qsTr("Library - Leaflit")
               : readerController.title.length > 0
                 ? readerController.title + qsTr(" - Leaflit")
                 : qsTr("Leaflit")

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

    onClosing: {
        readerWorkspace.flushReadingState()
        root.localStateStore.sync()
    }

    Binding {
        target: Theme
        property: "darkMode"
        value: root.localStateStore.darkMode
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

        anchors.fill: parent
        visible: !root.showingLibrary
        readerController: root.readerController
        settingsStore: root.localStateStore
        documentFormatter: root.readingDocumentFormatter
        onOpenRequested: openDialog.open()
    }

    LibraryView {
        anchors.fill: parent
        visible: root.showingLibrary
        libraryModel: root.libraryModel
        errorMessage: root.readerController.errorMessage
        onAddRequested: openDialog.open()
        onOpenRequested: sourceUrl => root.openBook(sourceUrl)
    }
}
