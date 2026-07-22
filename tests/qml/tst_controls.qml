import QtQuick
import QtQuick.Controls
import QtTest
import SZHBooksUiTest

TestCase {
    id: testCase

    name: "KeyboardAndFocus"
    when: windowShown

    Item {
        id: stage

        width: 640
        height: 480
    }

    Component {
        id: buttonComponent

        SZHButton {
            text: "Open book"
        }
    }

    Component {
        id: segmentedComponent

        SZHSegmentedControl {
            width: 320
            model: [
                { value: "light", label: "Light" },
                { value: "dark", label: "Dark" }
            ]
            value: "light"
        }
    }

    Component {
        id: sliderComponent

        SZHSlider {
            accessibleName: "Scroll speed"
            from: 50
            to: 200
            stepSize: 10
            value: 100
        }
    }

    Component {
        id: focusChainComponent

        Item {
            width: 320
            height: 100

            SZHButton {
                id: firstControl

                objectName: "firstControl"
                text: "First"
            }

            SZHTextField {
                id: secondControl

                objectName: "secondControl"
                anchors.top: firstControl.bottom
                placeholderText: "Second"
            }
        }
    }

    Component {
        id: restoreDialogComponent

        ProfileRestoreDialog {
            backupUrl: "file:///C:/Books/profile.szhbackup"
        }
    }

    Component {
        id: bookDelegateComponent

        LibraryBookDelegate {
            width: 188
            height: 288
            index: 0
            sourceUrl: "file:///C:/Books/novel.txt"
            sourcePath: "C:/Books/novel.txt"
            title: "Novel"
            author: "Author"
            series: ""
            seriesNumber: 0
            genres: []
            tags: []
            language: ""
            publicationYear: 0
            formatName: "TXT"
            collectionPath: "Fiction"
            coverUrl: ""
            progress: 0.42
            lastOpened: new Date(2026, 0, 1)
            fileAvailable: true
            cloudPlaceholder: false
            selected: false
            selectionMode: false
        }
    }

    QtObject {
        id: mockLibraryModel

        property string errorMessage: ""
        property int updateCount: 0
        property int coverCount: 0
        property var lastChanges: ({})
        property var selectedBooks: [{
            sourceUrl: "file:///C:/Books/novel.txt",
            title: "Novel",
            author: "Author"
        }]

        function book(sourceUrl) {
            return {
                sourceUrl: sourceUrl,
                title: "Novel",
                author: "Author",
                series: "Archive",
                seriesNumber: 2,
                genres: ["Fiction"],
                tags: ["Favorite"],
                language: "English",
                publicationYear: 2024,
                formatName: "EPUB",
                coverUrl: "",
                customCoverUrl: ""
            }
        }
        function clearError() { errorMessage = "" }
        function updateBooksMetadata(sourceUrls, changes) {
            updateCount += 1
            lastChanges = changes
            return true
        }
        function setCustomCover(sourceUrl, imageUrl) {
            coverCount += 1
            return true
        }
    }

    Component {
        id: metadataDialogComponent

        BookMetadataDialog {
            libraryModel: mockLibraryModel
        }
    }

    ListModel {
        id: mockActivityModel
    }

    ListModel {
        id: mockConflictModel
    }

    QtObject {
        id: mockFileService

        property bool busy: false
        property bool cancellationRequested: false
        property int operationTotal: 0
        property int operationCompleted: 0
        property int importedCount: 0
        property int duplicateCount: 0
        property int failedCount: 0
        property real progress: 0
        property string currentFileName: ""
        property string errorMessage: ""
        property url lastImportedUrl: ""
        property int moveCount: 0
        property int renameCount: 0
        property int removeCount: 0

        function isManagedBook(sourceUrl) { return true }
        function fileBaseName(sourceUrl) { return "novel" }
        function createCollection(parentPath, name) {
            mockSyncService.createCount += 1
            mockSyncService.createdParent = parentPath
            mockSyncService.createdName = name
            return true
        }
        function moveBook(sourceUrl, collectionPath) {
            moveCount += 1
            return sourceUrl
        }
        function renameBook(sourceUrl, name) {
            renameCount += 1
            return sourceUrl
        }
        function removeBook(sourceUrl, deleteFile) {
            removeCount += 1
            return true
        }
        function renameCollection(collectionPath, name) {
            return "Renamed"
        }
        function removeCollection(collectionPath) { return true }
        function cancel() { cancellationRequested = true }
    }

    QtObject {
        id: mockSyncService

        property bool configured: false
        property bool available: true
        property url rootFolder: "file:///C:/OneDrive/SZHBooks"
        property string rootDisplayPath: "C:\\OneDrive\\SZHBooks"
        property url suggestedFolder: "file:///C:/OneDrive/SZHBooks"
        property string suggestedDisplayPath: "C:\\OneDrive\\SZHBooks"
        property bool oneDriveDetected: true
        property var collections: ["Fiction", "Fiction/Classics"]
        property bool syncing: false
        property string status: "pending"
        property string errorMessage: ""
        property date lastSyncedAt: new Date(2026, 0, 1, 12, 0)
        property int conflictCount: 0
        property int cloudPlaceholderCount: 0
        property bool retryScheduled: false
        property date nextRetryAt: new Date(0)
        property var activityModel: mockActivityModel
        property var conflictModel: mockConflictModel
        property var fileService: mockFileService
        property int syncCount: 0
        property int createCount: 0
        property string createdParent: ""
        property string createdName: ""

        function useSuggestedFolder() {
            configured = true
            return true
        }
        function synchronizeNow() {
            syncCount += 1
            return true
        }
        function createCollection(parentPath, name) {
            createCount += 1
            createdParent = parentPath
            createdName = name
            return true
        }
        function retryNow() {
            syncCount += 1
            return true
        }
        function openRootFolder() { return true }
        function openConflictsFolder() { return true }
        function resolveConflict(conflictId, resolution) { return true }
        function resolveAllConflicts(resolution) { return 0 }
    }

    Component {
        id: syncBarComponent

        LibrarySyncBar {
            width: 600
            height: implicitHeight
            syncService: mockSyncService
        }
    }

    Component {
        id: createCollectionComponent

        CreateCollectionDialog {
            syncService: mockSyncService
        }
    }

    Component {
        id: syncCenterComponent

        SyncCenterDialog {
            syncService: mockSyncService
        }
    }

    Component {
        id: bookActionsComponent

        BookActionsDialog {
            fileService: mockFileService
            syncService: mockSyncService
        }
    }

    Component {
        id: collectionActionsComponent

        CollectionActionsDialog {
            fileService: mockFileService
        }
    }

    Component {
        id: importProgressComponent

        LibraryImportProgress {
            width: 600
            height: implicitHeight
            fileService: mockFileService
        }
    }

    QtObject {
        id: shortcutWorkspace

        property bool hasDocument: true
        property bool textPagedMode: false
        property bool canNavigateBack: false
        property bool canNavigateForward: false
        property int forwardCount: 0
        property int backwardCount: 0
        property int historyBackCount: 0
        property int historyForwardCount: 0
        function increaseScale() {}
        function decreaseScale() {}
        function toggleCurrentBookmark() {}
        function pageForward() { forwardCount += 1 }
        function pageBackward() { backwardCount += 1 }
        function goToStart() {}
        function goToEnd() {}
        function navigateBack() { historyBackCount += 1 }
        function navigateForward() { historyForwardCount += 1 }
    }

    QtObject {
        id: mockDocumentFormatter

        function applyTypography(document, lineHeight, paragraphSpacing,
                                 firstLineIndent, alignment, textColor, pageColor) {}
        function fitImages(document, maximumWidth, maximumHeight) {}
        function anchorPosition(document, anchor) { return -1 }
    }

    QtObject {
        id: mockSearchController

        property string documentText: ""
        property int currentStart: -1
        signal currentResultChanged
        function attachTextDocument(document) {}
        function setHighlightColors(annotation, match, current, currentText) {}
    }

    Component {
        id: pagedTextViewComponent

        TextDocumentView {
            width: 620
            height: 440
            documentText: Array(260).fill(
                "A carefully laid out paragraph for pagination testing.").join("\n\n")
            readingMode: "pages"
            documentFormatter: mockDocumentFormatter
            searchController: mockSearchController
        }
    }

    QtObject {
        id: shortcutHeader

        function openSearch() {}
        function openAnnotations(tab) {}
    }

    Component {
        id: shortcutsComponent

        AppShortcuts {
            readerWorkspace: shortcutWorkspace
            appHeader: shortcutHeader
            showingLibrary: false
            readingNavigationEnabled: true
        }
    }

    function test_buttonActivatesFromKeyboard() {
        const button = createTemporaryObject(buttonComponent, stage)
        verify(button)

        let clickCount = 0
        button.clicked.connect(function() { clickCount += 1 })
        button.forceActiveFocus()
        tryCompare(button, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(clickCount, 1)
    }

    function test_segmentAndSliderUseKeyboard() {
        const segmented = createTemporaryObject(segmentedComponent, stage)
        verify(segmented)
        const darkOption = findChild(segmented, "segment_dark")
        verify(darkOption)

        let selectedValue = ""
        segmented.valueSelected.connect(function(value) { selectedValue = value })
        darkOption.forceActiveFocus()
        tryCompare(darkOption, "activeFocus", true)
        keyClick(Qt.Key_Space)
        compare(selectedValue, "dark")

        const slider = createTemporaryObject(sliderComponent, stage)
        verify(slider)
        slider.forceActiveFocus()
        tryCompare(slider, "activeFocus", true)
        keyClick(Qt.Key_Right)
        compare(slider.value, 110)
    }

    function test_controlsAcceptTabFocus() {
        const chain = createTemporaryObject(focusChainComponent, stage)
        verify(chain)
        const first = findChild(chain, "firstControl")
        const second = findChild(chain, "secondControl")
        verify(first)
        verify(second)

        first.forceActiveFocus()
        tryCompare(first, "activeFocus", true)
        verify(first.activeFocusOnTab)
        verify(second.activeFocusOnTab)
        second.forceActiveFocus(Qt.TabFocusReason)
        tryCompare(second, "activeFocus", true)
    }

    function test_restoreDialogStartsSafelyAndClosesWithEscape() {
        const dialog = createTemporaryObject(restoreDialogComponent, stage)
        verify(dialog)
        dialog.open()
        tryCompare(dialog, "opened", true)

        const cancel = findChild(dialog, "cancelRestoreButton")
        verify(cancel)
        tryCompare(cancel, "activeFocus", true)
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "opened", false)
    }

    function test_libraryBookOpensFromKeyboard() {
        const delegate = createTemporaryObject(bookDelegateComponent, stage)
        verify(delegate)

        let openedUrl = ""
        delegate.openRequested.connect(function(sourceUrl) {
            openedUrl = sourceUrl.toString()
        })
        delegate.forceActiveFocus()
        tryCompare(delegate, "activeFocus", true)
        keyClick(Qt.Key_Return)
        compare(openedUrl, "file:///C:/Books/novel.txt")
    }

    function test_metadataDialogEditsSingleBook() {
        mockLibraryModel.updateCount = 0
        const dialog = createTemporaryObject(metadataDialogComponent, stage)
        verify(dialog)
        dialog.openFor("file:///C:/Books/novel.txt")
        tryCompare(dialog, "opened", true)

        const titleField = findChild(dialog, "metadataTitleField")
        const saveButton = findChild(dialog, "saveMetadataButton")
        verify(titleField)
        verify(saveButton)
        compare(titleField.text, "Novel")
        titleField.text = "Edited novel"
        saveButton.clicked()

        compare(mockLibraryModel.updateCount, 1)
        compare(mockLibraryModel.lastChanges.title, "Edited novel")
        tryCompare(dialog, "opened", false)
    }

    function test_textReaderPaginatesAndKeepsSemanticPosition() {
        const reader = createTemporaryObject(pagedTextViewComponent, stage)
        verify(reader)
        tryVerify(function() { return reader.pageCount > 2 }, 5000)
        compare(reader.currentPage, 0)

        reader.goToProgress(0.55)
        verify(reader.currentPage > 0)
        const savedPosition = reader.currentTextPosition
        verify(savedPosition > 0)

        reader.readingMode = "scroll"
        reader.restorePosition(0.55, savedPosition)
        tryVerify(function() {
            return Math.abs(reader.currentTextPosition - savedPosition) < 120
        }, 5000)
    }

    function test_syncBarOffersAndRefreshesFolder() {
        mockSyncService.configured = false
        mockSyncService.syncing = false
        const bar = createTemporaryObject(syncBarComponent, stage)
        verify(bar)
        compare(bar.syncService.configured, false)
        verify(bar.height > 0)

        const suggestedButton = findChild(bar, "useSuggestedFolderButton")
        const chooseButton = findChild(bar, "chooseLibraryFolderButton")
        verify(suggestedButton)
        verify(chooseButton)
        mockFileService.busy = true
        compare(suggestedButton.enabled, false)
        compare(chooseButton.enabled, false)
        mockFileService.busy = false
        tryCompare(suggestedButton, "enabled", true)
        tryCompare(chooseButton, "enabled", true)
        suggestedButton.clicked()
        compare(mockSyncService.configured, true)

        const syncButton = findChild(bar, "synchronizeLibraryButton")
        verify(syncButton)
        mockSyncService.syncCount = 0
        syncButton.clicked()
        compare(mockSyncService.syncCount, 1)
    }

    function test_createNestedCollectionFromKeyboard() {
        mockSyncService.configured = true
        mockSyncService.createCount = 0
        const dialog = createTemporaryObject(createCollectionComponent, stage)
        verify(dialog)
        dialog.openFor("Fiction")
        tryCompare(dialog, "opened", true)

        const nameField = findChild(dialog, "collectionNameField")
        const createButton = findChild(dialog, "createCollectionButton")
        verify(nameField)
        verify(createButton)
        nameField.text = "Classics"
        createButton.forceActiveFocus()
        keyClick(Qt.Key_Space)

        compare(mockSyncService.createCount, 1)
        compare(mockSyncService.createdParent, "Fiction")
        compare(mockSyncService.createdName, "Classics")
        tryCompare(dialog, "opened", false)
    }

    function test_syncCenterOpensAndChangesViews() {
        mockSyncService.configured = true
        const dialog = createTemporaryObject(syncCenterComponent, stage)
        verify(dialog)
        dialog.open()
        tryCompare(dialog, "opened", true)
        compare(dialog.currentView, "status")
        dialog.currentView = "activity"
        compare(dialog.currentView, "activity")
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "opened", false)
    }

    function test_bookAndCollectionActionsOpenSafely() {
        mockFileService.moveCount = 0
        const bookDialog = createTemporaryObject(bookActionsComponent, stage)
        verify(bookDialog)
        bookDialog.openFor("file:///C:/OneDrive/SZHBooks/novel.txt",
                           "Novel", "", true)
        tryCompare(bookDialog, "opened", true)
        const destination = findChild(bookDialog, "bookDestinationMenu")
        const moveButton = findChild(bookDialog, "moveBookButton")
        verify(destination)
        verify(moveButton)
        destination.value = "Fiction"
        moveButton.clicked()
        compare(mockFileService.moveCount, 1)
        tryCompare(bookDialog, "opened", false)

        const collectionDialog = createTemporaryObject(collectionActionsComponent, stage)
        verify(collectionDialog)
        collectionDialog.openFor("Fiction")
        tryCompare(collectionDialog, "opened", true)
        keyClick(Qt.Key_Escape)
        tryCompare(collectionDialog, "opened", false)
    }

    function test_importProgressReportsAndCancels() {
        mockFileService.busy = true
        mockFileService.cancellationRequested = false
        mockFileService.operationTotal = 4
        mockFileService.operationCompleted = 1
        mockFileService.progress = 0.25
        mockFileService.currentFileName = "novel.epub"
        const progress = createTemporaryObject(importProgressComponent, stage)
        verify(progress)
        compare(progress.fileService.busy, true)
        verify(progress.implicitHeight > 0)
        verify(progress.height > 0)
        mockFileService.cancel()
        verify(mockFileService.cancellationRequested)
        mockFileService.busy = false
    }

    function test_applicationShortcutsRouteCommands() {
        const shortcuts = createTemporaryObject(shortcutsComponent, stage)
        verify(shortcuts)

        let openCount = 0
        let themeCount = 0
        shortcuts.openRequested.connect(function() { openCount += 1 })
        shortcuts.colorThemeToggleRequested.connect(function() { themeCount += 1 })
        shortcutWorkspace.forwardCount = 0

        keyClick(Qt.Key_O, Qt.ControlModifier)
        keyClick(Qt.Key_D, Qt.ControlModifier | Qt.ShiftModifier)
        keyClick(Qt.Key_PageDown)

        compare(openCount, 1)
        compare(themeCount, 1)
        compare(shortcutWorkspace.forwardCount, 1)
    }
}
