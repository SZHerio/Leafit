import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: root

    required property var readerController
    required property var localStateStore
    required property var readingDocumentFormatter

    width: 1120
    height: 760
    minimumWidth: 760
    minimumHeight: 560
    visible: true
    title: readerController.title.length > 0
               ? readerController.title + qsTr(" - Leaflit")
               : qsTr("Leaflit")

    color: Theme.windowColor

    function openBook(fileUrl) {
        root.readerController.openFile(fileUrl)
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
        onOpenRequested: openDialog.open()
        onDarkModeToggled: root.localStateStore.darkMode = darkMode
    }

    footer: ReaderStatusBar {
        readerController: root.readerController
        readerWorkspace: readerWorkspace
    }

    ReaderWorkspace {
        id: readerWorkspace

        anchors.fill: parent
        readerController: root.readerController
        settingsStore: root.localStateStore
        documentFormatter: root.readingDocumentFormatter
        onOpenRequested: openDialog.open()
    }
}
