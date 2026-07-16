import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Window

ApplicationWindow {
    id: root

    required property var readerController
    required property var localStateStore
    required property var libraryModel
    required property var readingDocumentFormatter
    required property var readingSearchController
    required property var readingAnnotationStore

    property bool showingLibrary: true
    property bool focusMode: false
    property int visibilityBeforeFocus: Window.Windowed
    property url relinkSourceUrl
    readonly property bool readingNavigationEnabled: !root.showingLibrary
                                                      && readerWorkspace.hasDocument
                                                      && !appHeader.readerPopupOpen
                                                      && !readerWorkspace.sidebarTextInputActive
    readonly property var supportedBookNameFilters: [
        qsTr("Supported books (*.txt *.fb2 *.epub *.pdf *.html *.htm *.md *.markdown *.docx)"),
        qsTr("Text files (*.txt *.md *.markdown *.html *.htm)"),
        qsTr("Electronic books (*.fb2 *.epub)"),
        qsTr("PDF files (*.pdf)"),
        qsTr("Word documents (*.docx)")
    ]

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
        root.exitFocusMode()
        readerWorkspace.flushReadingState()
        root.localStateStore.sync()
        root.libraryModel.refresh()
        root.showingLibrary = true
    }

    function toggleColorTheme() {
        root.localStateStore.darkMode = !root.localStateStore.darkMode
    }

    function enterFocusMode() {
        if (root.focusMode || root.showingLibrary || !readerWorkspace.hasDocument) {
            return
        }
        root.visibilityBeforeFocus = root.visibility === Window.Maximized
                                     ? Window.Maximized : Window.Windowed
        root.focusMode = true
        root.showFullScreen()
    }

    function exitFocusMode() {
        if (!root.focusMode) {
            return
        }
        root.focusMode = false
        if (root.visibilityBeforeFocus === Window.Maximized) {
            root.showMaximized()
        } else {
            root.showNormal()
        }
    }

    function toggleFocusMode() {
        if (root.focusMode) {
            root.exitFocusMode()
        } else {
            root.enterFocusMode()
        }
    }

    function locateBook(sourceUrl) {
        root.libraryModel.clearError()
        root.relinkSourceUrl = sourceUrl
        relinkDialog.open()
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

    Shortcut {
        enabled: !root.showingLibrary && readerWorkspace.hasDocument
        sequence: "F11"
        onActivated: root.toggleFocusMode()
    }

    Shortcut {
        enabled: root.focusMode
        sequence: "Esc"
        onActivated: root.exitFocusMode()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "PgUp"
        onActivated: readerWorkspace.pageBackward()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "PgDown"
        onActivated: readerWorkspace.pageForward()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "Home"
        onActivated: readerWorkspace.goToStart()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "End"
        onActivated: readerWorkspace.goToEnd()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "Space"
        onActivated: readerWorkspace.pageForward()
    }

    Shortcut {
        enabled: root.readingNavigationEnabled
        sequence: "Shift+Space"
        onActivated: readerWorkspace.pageBackward()
    }

    FileDialog {
        id: openDialog

        title: qsTr("Open book")
        nameFilters: root.supportedBookNameFilters

        onAccepted: root.openBook(selectedFile)
    }

    FileDialog {
        id: relinkDialog

        title: qsTr("Locate moved book")
        nameFilters: root.supportedBookNameFilters
        onAccepted: {
            if (root.libraryModel.relinkBook(root.relinkSourceUrl, selectedFile))
                root.relinkSourceUrl = ""
        }
        onRejected: root.relinkSourceUrl = ""
    }

    header: AppHeader {
        id: appHeader
        objectName: "appHeader"

        readerController: root.readerController
        readerWorkspace: readerWorkspace
        settingsStore: root.localStateStore
        darkMode: root.localStateStore.darkMode
        showingLibrary: root.showingLibrary
        visible: !root.focusMode
        onOpenRequested: openDialog.open()
        onLibraryRequested: root.showLibrary()
        onDarkModeToggled: root.localStateStore.darkMode = darkMode
        onFocusModeRequested: root.toggleFocusMode()
    }

    footer: ReaderStatusBar {
        readerController: root.readerController
        readerWorkspace: readerWorkspace
        visible: !root.showingLibrary && !root.focusMode
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
        onRelinkRequested: sourceUrl => root.locateBook(sourceUrl)

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionNormal
            }
        }
    }

    SZHIconButton {
        z: 90
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spaceMd
        anchors.rightMargin: Theme.spaceMd
        visible: root.focusMode && !root.showingLibrary
        opacity: focusExitHover.hovered || activeFocus ? 1 : 0.42
        symbol: "\u26f6"
        symbolPixelSize: Theme.bodyLargeFontSize
        toolTip: qsTr("Exit focus mode (F11)")
        onClicked: root.exitFocusMode()

        HoverHandler {
            id: focusExitHover
        }

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.motionFast
            }
        }
    }

    SZHNotification {
        id: readerErrorNotification

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

    SZHNotification {
        z: 101
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: Theme.spaceMd
                           + (readerErrorNotification.visible
                              ? readerErrorNotification.height + Theme.spaceSm
                              : 0)
        anchors.rightMargin: Theme.spaceMd
        width: Math.min(480, Math.max(320, root.width - Theme.spaceXl))
        shown: root.libraryModel.errorMessage.length > 0
        heading: qsTr("Could not update library")
        message: root.libraryModel.errorMessage
        actionText: qsTr("Choose another")
        onActionRequested: relinkDialog.open()
        onDismissed: root.libraryModel.clearError()
    }
}
