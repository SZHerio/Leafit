import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    required property var readerController
    required property var readerWorkspace
    required property var settingsStore
    property bool darkMode: false
    property bool showingLibrary: false
    readonly property bool compactMode: width < 1080
    readonly property bool readerPopupOpen: searchPopup.opened
                                                || readingSettings.opened
                                                || readerTools.opened

    signal openRequested
    signal libraryRequested
    signal darkModeToggled(bool darkMode)
    signal focusModeRequested

    function closeReaderPopups() {
        searchPopup.close()
        readingSettings.close()
        readerTools.close()
    }

    function openSearch() {
        if (root.showingLibrary || !root.readerWorkspace.hasDocument) {
            return
        }
        root.closeReaderPopups()
        searchPopup.openAndFocus()
    }

    function openAnnotations(tab) {
        if (root.showingLibrary || !root.readerWorkspace.hasDocument) {
            return
        }
        root.closeReaderPopups()
        root.readerWorkspace.openSidebar(tab === "highlights" ? "notes" : "bookmarks")
    }

    function toggleChapterNavigation() {
        root.closeReaderPopups()
        root.readerWorkspace.toggleSidebar("contents")
    }

    function toggleReadingSettings() {
        const wasOpen = readingSettings.opened
        root.closeReaderPopups()
        if (!wasOpen) {
            readingSettings.open()
        }
    }

    function toggleReaderTools() {
        const wasOpen = readerTools.opened
        root.closeReaderPopups()
        if (!wasOpen) {
            readerTools.open()
        }
    }

    implicitHeight: Theme.toolbarHeight
    color: Theme.chromeColor

    onShowingLibraryChanged: {
        if (root.showingLibrary) {
            root.closeReaderPopups()
            root.readerWorkspace.closeSidebar()
        }
    }

    Connections {
        target: root.readerWorkspace

        function onHasChaptersChanged() {
            if (!root.readerWorkspace.hasChapters
                    && root.readerWorkspace.sidebarTab === "contents") {
                root.readerWorkspace.closeSidebar()
            }
        }

        function onHasDocumentChanged() {
            if (!root.readerWorkspace.hasDocument) {
                root.closeReaderPopups()
            }
        }
    }

    Behavior on color {
        ColorAnimation {
            duration: Theme.motionNormal
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceLg
        anchors.rightMargin: Theme.spaceLg
        spacing: Theme.spaceSm

        SZHBrandLogo {
            iconOnly: root.width < 1080
            darkVariant: Theme.darkMode
            Layout.preferredWidth: iconOnly ? 42 : 176
            Layout.preferredHeight: 42
            Layout.rightMargin: Theme.spaceXs
        }

        SZHIconButton {
            symbol: "\u25a6"
            toolTip: qsTr("Library")
            onChrome: true
            selected: root.showingLibrary
            onClicked: root.libraryRequested()
        }

        SZHButton {
            visible: root.showingLibrary || !root.compactMode
            text: root.showingLibrary ? qsTr("Add book") : qsTr("Open book")
            variant: "chrome"
            onClicked: {
                root.closeReaderPopups()
                root.openRequested()
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.spaceSm
            Layout.rightMargin: Theme.spaceSm
            text: root.showingLibrary
                  ? qsTr("Library")
                  : root.readerController.title
                    + (root.readerController.author.length > 0
                       ? qsTr("  \u00b7  ") + root.readerController.author
                       : "")
            color: Theme.chromeMutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.bodyFontSize
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }

        Rectangle {
            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.hasDocument
            Layout.preferredWidth: 1
            Layout.preferredHeight: 24
            Layout.leftMargin: Theme.spaceXs
            Layout.rightMargin: Theme.spaceXs
            color: Theme.chromeBorderColor
        }

        SZHIconButton {
            id: searchButton

            visible: !root.showingLibrary && root.readerWorkspace.hasDocument
            symbol: "\u2315"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: qsTr("Search in book (Ctrl+F)")
            onChrome: true
            selected: searchPopup.opened
            onClicked: searchPopup.opened ? searchPopup.close() : root.openSearch()
        }

        SZHIconButton {
            visible: !root.showingLibrary && root.readerWorkspace.hasDocument
            symbol: root.readerWorkspace.currentLocationBookmarked ? "\u2605" : "\u2606"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: root.readerWorkspace.currentLocationBookmarked
                     ? qsTr("Remove bookmark (Ctrl+B)")
                     : qsTr("Add bookmark (Ctrl+B)")
            onChrome: true
            selected: root.readerWorkspace.currentLocationBookmarked
            onClicked: root.readerWorkspace.toggleCurrentBookmark()
        }

        SZHIconButton {
            id: annotationsButton

            visible: !root.showingLibrary && root.readerWorkspace.hasDocument
            symbol: "\u2261"
            toolTip: qsTr("Reading marks")
            onChrome: true
            selected: root.readerWorkspace.sidebarOpen
                      && (root.readerWorkspace.sidebarTab === "bookmarks"
                          || root.readerWorkspace.sidebarTab === "notes")
            onClicked: root.readerWorkspace.toggleSidebar("bookmarks")
        }

        SZHIconButton {
            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.hasDocument
            symbol: "-"
            toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom out")
                         : qsTr("Decrease font size")
            onChrome: true
            enabled: root.readerWorkspace.canDecreaseScale
            onClicked: root.readerWorkspace.decreaseScale()
        }

        Label {
            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.hasDocument
            text: root.readerWorkspace.scaleLabel
            color: Theme.chromeMutedTextColor
            font.family: Theme.uiFontFamily
            font.pixelSize: Theme.captionFontSize
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            Layout.preferredWidth: 58
        }

        SZHIconButton {
            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.hasDocument
            symbol: "+"
            toolTip: root.readerWorkspace.showingPdf
                         ? qsTr("Zoom in")
                         : qsTr("Increase font size")
            onChrome: true
            enabled: root.readerWorkspace.canIncreaseScale
            onClicked: root.readerWorkspace.increaseScale()
        }

        SZHIconButton {
            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.showingPdf
            symbol: "\u2922"
            toolTip: qsTr("Fit page to width")
            onChrome: true
            onClicked: root.readerWorkspace.fitPdfToWidth()
        }

        SZHIconButton {
            id: chapterButton

            visible: !root.compactMode
                     && !root.showingLibrary
                     && root.readerWorkspace.hasChapters
            symbol: "\u2630"
            toolTip: qsTr("Chapters")
            onChrome: true
            selected: root.readerWorkspace.sidebarOpen
                      && root.readerWorkspace.sidebarTab === "contents"
            onClicked: root.toggleChapterNavigation()
        }

        SZHIconButton {
            visible: !root.compactMode && !root.showingLibrary
            symbol: "\u2699"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: qsTr("Reading settings")
            onChrome: true
            selected: readingSettings.opened
            onClicked: root.toggleReadingSettings()
        }

        SZHIconButton {
            id: overflowButton

            visible: root.compactMode && !root.showingLibrary
            symbol: "\u22ef"
            symbolPixelSize: Theme.titleFontSize
            toolTip: qsTr("Reader tools")
            onChrome: true
            selected: readerTools.opened
            onClicked: root.toggleReaderTools()
        }

        SZHIconButton {
            visible: !root.showingLibrary && root.readerWorkspace.hasDocument
            symbol: "\u26f6"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: qsTr("Focus mode (F11)")
            onChrome: true
            onClicked: root.focusModeRequested()
        }

        SZHIconButton {
            symbol: root.darkMode ? "\u2600" : "\u263e"
            symbolPixelSize: Theme.bodyLargeFontSize
            toolTip: root.darkMode
                     ? qsTr("Use light theme (Ctrl+Shift+D)")
                     : qsTr("Use dark theme (Ctrl+Shift+D)")
            onChrome: true
            onClicked: root.darkModeToggled(!root.darkMode)
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.chromeBorderColor
    }

    ReadingSettingsPopup {
        id: readingSettings

        parent: root
        x: Math.max(Theme.spaceMd, root.width - width - Theme.spaceLg)
        y: root.height + Theme.spaceXs
        settingsStore: root.settingsStore
        textSettingsAvailable: root.readerWorkspace.showingText
    }

    SearchPopup {
        id: searchPopup

        parent: root
        x: Math.max(Theme.spaceMd, root.width - width - Theme.spaceLg)
        y: root.height + Theme.spaceXs
        readerWorkspace: root.readerWorkspace
        onAboutToShow: {
            const buttonPosition = searchButton.mapToItem(root, 0, 0)
            x = Math.max(Theme.spaceMd,
                         Math.min(root.width - width - Theme.spaceMd,
                                  buttonPosition.x + searchButton.width - width))
        }
    }

    ReaderToolsPopup {
        id: readerTools

        parent: root
        x: Math.max(Theme.spaceMd, root.width - width - Theme.spaceLg)
        y: root.height + Theme.spaceXs
        readerWorkspace: root.readerWorkspace
        onAboutToShow: {
            const buttonPosition = overflowButton.mapToItem(root, 0, 0)
            x = Math.max(Theme.spaceMd,
                         Math.min(root.width - width - Theme.spaceMd,
                                  buttonPosition.x + overflowButton.width - width))
        }
        onOpenRequested: {
            root.closeReaderPopups()
            root.openRequested()
        }
        onChaptersRequested: root.toggleChapterNavigation()
        onSettingsRequested: root.toggleReadingSettings()
    }

}
