cmake_minimum_required(VERSION 3.16)

project(SZHBooks VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

if(WIN32)
    set(CMAKE_INSTALL_BINDIR ".")
endif()

find_package(Qt6 REQUIRED COMPONENTS Concurrent Gui LinguistTools Pdf PdfQuick Quick QuickControls2 QuickDialogs2 Xml)

qt_standard_project_setup(REQUIRES 6.8)

set_source_files_properties(ui/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

add_library(szhbooks_miniz STATIC
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)

include(GenerateExportHeader)
generate_export_header(szhbooks_miniz
    BASE_NAME miniz
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h
)

target_include_directories(szhbooks_miniz
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

add_subdirectory(cmake/application)

target_sources(appSZHBooks PRIVATE
    archive/ziparchivereader.cpp
    archive/ziparchivereader.h
    archive/ziparchivewriter.cpp
    archive/ziparchivewriter.h
    archive/zippathutils.cpp
    archive/zippathutils.h
    document/docxcontentrenderer.cpp
    document/docxcontentrenderer.h
    document/documentlimits.h
    document/epubcontentrenderer.cpp
    document/epubcontentrenderer.h
    document/fb2contentrenderer.cpp
    document/fb2contentrenderer.h
    document/richtextcontent.h
    document/richtextprocessor.cpp
    document/richtextprocessor.h
    document/xmlutils.cpp
    document/xmlutils.h
    documentloadresult.h
    documentreader.h
    documentreaders.cpp
    documentreaders.h
    library/bookcoverprovider.cpp
    library/bookcoverprovider.h
    library/bookmetadata.h
    library/bookmetadataservice.cpp
    library/bookmetadataservice.h
    library/cloudfileavailability.cpp
    library/cloudfileavailability.h
    library/librarybook.h
    library/libraryfileservice.cpp
    library/libraryfileservice.h
    library/librarymodel.cpp
    library/librarymodel.h
    library/libraryrepository.cpp
    library/libraryrepository.h
    localization/localizationcontroller.cpp
    localization/localizationcontroller.h
    localdocumentloader.cpp
    localdocumentloader.h
    reader/readingannotation.h
    reader/readingdocumentformatter.cpp
    reader/readingdocumentformatter.h
    reader/readingsearchcontroller.cpp
    reader/readingsearchcontroller.h
    readercontroller.cpp
    readercontroller.h
    storage/documentstoragekey.cpp
    storage/documentstoragekey.h
    storage/localstatestore.cpp
    storage/localstatestore.h
    storage/profilearchiveservice.cpp
    storage/profilearchiveservice.h
    storage/profilestorage.h
    storage/readingannotationstore.cpp
    storage/readingannotationstore.h
    sync/onedrivelibraryservice.cpp
    sync/onedrivelibraryservice.h
    sync/portableprofilemapper.cpp
    sync/portableprofilemapper.h
    sync/profilesyncengine.cpp
    sync/profilesyncengine.h
    sync/syncactivitymodel.cpp
    sync/syncactivitymodel.h
    sync/syncconfigurationstore.cpp
    sync/syncconfigurationstore.h
    sync/syncconflictmodel.cpp
    sync/syncconflictmodel.h
)

target_include_directories(appSZHBooks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

qt_add_qml_module(appSZHBooks
    URI SZHBooks
    NO_GENERATE_QMLTYPES
    QML_FILES
        Main.qml
        library/BookActionsDialog.qml
        library/CollectionActionsDialog.qml
        library/LibraryBookCover.qml
        library/LibraryBookDelegate.qml
        library/LibraryBookListDelegate.qml
        library/LibraryContinueCard.qml
        library/CreateCollectionDialog.qml
        library/LibraryCollectionSidebar.qml
        library/LibraryControls.qml
        library/LibraryImportProgress.qml
        library/LibrarySyncBar.qml
        library/LibraryView.qml
        library/SyncCenterDialog.qml
        reader/EmptyReaderView.qml
        reader/PageNavigationControl.qml
        reader/PdfDocumentView.qml
        reader/PdfPasswordDialog.qml
        reader/ReaderSidebar.qml
        reader/ReaderWorkspace.qml
        reader/SearchPopup.qml
        reader/SelectionActionBar.qml
        reader/TextNavigationControl.qml
        reader/TextDocumentView.qml
        shell/AppHeader.qml
        shell/AppShortcuts.qml
        shell/ProfileRestoreDialog.qml
        shell/ReaderToolsPopup.qml
        shell/ReadingSettingsPopup.qml
        shell/ReaderStatusBar.qml
        ui/SZHBrandLogo.qml
        ui/SZHButton.qml
        ui/SZHIconButton.qml
        ui/SZHMenuButton.qml
        ui/SZHNotification.qml
        ui/SZHProgressBar.qml
        ui/SZHScrollBar.qml
        ui/SZHSegmentedControl.qml
        ui/SZHSlider.qml
        ui/SZHSurface.qml
        ui/SZHSwitch.qml
        ui/SZHTextArea.qml
        ui/SZHTextField.qml
        ui/Theme.qml
    RESOURCES
        assets/branding/szhbooks-icon.png
        assets/branding/szhbooks-logo-sheet-transparent.png
)

qt_add_translations(appSZHBooks
    TS_FILES
        translations/SZHBooks_ru.ts
    RESOURCE_PREFIX "/i18n"
    LUPDATE_TARGET szhbooks_update_translations
    LRELEASE_TARGET szhbooks_release_translations
    LUPDATE_OPTIONS -no-obsolete
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appSZHBooks PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER com.szhbooks.reader
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME SZHBooks
    WIN32_EXECUTABLE TRUE
)

if(WIN32)
    enable_language(RC)
    set(SZHBOOKS_WINDOWS_ICON
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/szhbooks.ico")
    configure_file(
        packaging/windows/szhbooks.rc.in
        ${CMAKE_CURRENT_BINARY_DIR}/szhbooks.rc
        @ONLY
    )
    target_sources(appSZHBooks PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/szhbooks.rc)
endif()

target_link_libraries(appSZHBooks
    PRIVATE
        szhbooks_miniz
        Qt6::Concurrent
        Qt6::Gui
        Qt6::Pdf
        Qt6::PdfQuick
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Xml
)

target_compile_definitions(appSZHBooks PRIVATE
    SZHBOOKS_VERSION="${PROJECT_VERSION}"
)

if(WIN32)
    set(SZHBOOKS_RUNTIME_INSTALL_DIR ".")
else()
    set(SZHBOOKS_RUNTIME_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}")
endif()

install(TARGETS appSZHBooks
    COMPONENT szhbooks
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${SZHBOOKS_RUNTIME_INSTALL_DIR}
)

qt_finalize_executable(appSZHBooks)

qt_generate_deploy_qml_app_script(
    TARGET appSZHBooks
    OUTPUT_SCRIPT szhbooks_deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${szhbooks_deploy_script} COMPONENT szhbooks)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 QUIET COMPONENTS QuickTest Test)

    if(TARGET Qt6::Test)
        qt_add_executable(tst_localstatestore
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            library/bookcoverprovider.cpp
            library/bookcoverprovider.h
            library/bookmetadata.h
            library/bookmetadataservice.cpp
            library/bookmetadataservice.h
            library/cloudfileavailability.cpp
            library/cloudfileavailability.h
            library/librarybook.h
            library/librarymodel.cpp
            library/librarymodel.h
            library/libraryrepository.cpp
            library/libraryrepository.h
            reader/readingannotation.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            storage/readingannotationstore.cpp
            storage/readingannotationstore.h
            tests/localstatestoretest.cpp
        )

        target_link_libraries(tst_localstatestore
            PRIVATE
                szhbooks_miniz
                Qt6::Core
                Qt6::Gui
                Qt6::Pdf
                Qt6::Test
                Qt6::Xml
        )

        add_test(NAME localstatestore COMMAND tst_localstatestore)
        set_tests_properties(localstatestore PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_profilearchive
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            archive/ziparchivewriter.cpp
            archive/ziparchivewriter.h
            library/librarybook.h
            reader/readingannotation.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            storage/profilearchiveservice.cpp
            storage/profilearchiveservice.h
            storage/profilestorage.h
            storage/readingannotationstore.cpp
            storage/readingannotationstore.h
            tests/profilearchiveservicetest.cpp
        )

        target_link_libraries(tst_profilearchive
            PRIVATE
                szhbooks_miniz
                Qt6::Core
                Qt6::Test
        )

        add_test(NAME profilearchive COMMAND tst_profilearchive)
        set_tests_properties(profilearchive PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_onedrivesync
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            library/bookcoverprovider.cpp
            library/bookcoverprovider.h
            library/bookmetadata.h
            library/bookmetadataservice.cpp
            library/bookmetadataservice.h
            library/cloudfileavailability.cpp
            library/cloudfileavailability.h
            library/librarybook.h
            library/libraryfileservice.cpp
            library/libraryfileservice.h
            library/libraryrepository.cpp
            library/libraryrepository.h
            reader/readingannotation.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            storage/profilestorage.h
            storage/readingannotationstore.cpp
            storage/readingannotationstore.h
            sync/onedrivelibraryservice.cpp
            sync/onedrivelibraryservice.h
            sync/portableprofilemapper.cpp
            sync/portableprofilemapper.h
            sync/profilesyncengine.cpp
            sync/profilesyncengine.h
            sync/syncactivitymodel.cpp
            sync/syncactivitymodel.h
            sync/syncconfigurationstore.cpp
            sync/syncconfigurationstore.h
            sync/syncconflictmodel.cpp
            sync/syncconflictmodel.h
            tests/onedrivesynctest.cpp
        )

        target_link_libraries(tst_onedrivesync
            PRIVATE
                szhbooks_miniz
                Qt6::Concurrent
                Qt6::Core
                Qt6::Gui
                Qt6::Pdf
                Qt6::Test
                Qt6::Xml
        )

        add_test(NAME onedrivesync COMMAND tst_onedrivesync)
        set_tests_properties(onedrivesync PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_libraryfiles
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            library/bookcoverprovider.cpp
            library/bookcoverprovider.h
            library/bookmetadata.h
            library/bookmetadataservice.cpp
            library/bookmetadataservice.h
            library/cloudfileavailability.cpp
            library/cloudfileavailability.h
            library/librarybook.h
            library/libraryfileservice.cpp
            library/libraryfileservice.h
            library/libraryrepository.cpp
            library/libraryrepository.h
            reader/readingannotation.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            storage/readingannotationstore.cpp
            storage/readingannotationstore.h
            tests/libraryfileservicetest.cpp
        )

        target_link_libraries(tst_libraryfiles
            PRIVATE
                szhbooks_miniz
                Qt6::Concurrent
                Qt6::Core
                Qt6::Gui
                Qt6::Pdf
                Qt6::Test
                Qt6::Xml
        )

        add_test(NAME libraryfiles COMMAND tst_libraryfiles)
        set_tests_properties(libraryfiles PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_documentreaders
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            archive/zippathutils.cpp
            archive/zippathutils.h
            document/docxcontentrenderer.cpp
            document/docxcontentrenderer.h
            document/documentlimits.h
            document/epubcontentrenderer.cpp
            document/epubcontentrenderer.h
            document/fb2contentrenderer.cpp
            document/fb2contentrenderer.h
            document/richtextcontent.h
            document/richtextprocessor.cpp
            document/richtextprocessor.h
            document/xmlutils.cpp
            document/xmlutils.h
            documentloadresult.h
            documentreader.h
            documentreaders.cpp
            documentreaders.h
            tests/documentreaderstest.cpp
        )

        target_link_libraries(tst_documentreaders
            PRIVATE
                szhbooks_miniz
                Qt6::Gui
                Qt6::Pdf
                Qt6::Test
                Qt6::Xml
        )

        add_test(NAME documentreaders COMMAND tst_documentreaders)
        set_tests_properties(documentreaders PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_readingfeatures
            reader/readingannotation.h
            reader/readingsearchcontroller.cpp
            reader/readingsearchcontroller.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/readingannotationstore.cpp
            storage/readingannotationstore.h
            tests/readingfeaturestest.cpp
        )

        target_link_libraries(tst_readingfeatures
            PRIVATE
                Qt6::Gui
                Qt6::Quick
                Qt6::Test
        )

        add_test(NAME readingfeatures COMMAND tst_readingfeatures)
        set_tests_properties(readingfeatures PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_libraryservices
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
            library/bookcoverprovider.cpp
            library/bookcoverprovider.h
            library/bookmetadata.h
            library/bookmetadataservice.cpp
            library/bookmetadataservice.h
            tests/libraryservicestest.cpp
        )

        target_link_libraries(tst_libraryservices
            PRIVATE
                szhbooks_miniz
                Qt6::Gui
                Qt6::Pdf
                Qt6::Test
                Qt6::Xml
        )

        add_test(NAME libraryservices COMMAND tst_libraryservices)
        set_tests_properties(libraryservices PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_localizationcontroller
            localization/localizationcontroller.cpp
            localization/localizationcontroller.h
            library/librarybook.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            storage/profilestorage.h
            tests/localizationcontrollertest.cpp
        )

        target_link_libraries(tst_localizationcontroller
            PRIVATE
                Qt6::Gui
                Qt6::Qml
                Qt6::Test
        )
        target_include_directories(tst_localizationcontroller PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
        )

        qt_add_resources(tst_localizationcontroller "tst_localization_translations"
            PREFIX "/i18n"
            BASE "${CMAKE_CURRENT_BINARY_DIR}"
            FILES "${CMAKE_CURRENT_BINARY_DIR}/SZHBooks_ru.qm"
        )
        add_dependencies(tst_localizationcontroller szhbooks_release_translations)

        add_test(NAME localizationcontroller COMMAND tst_localizationcontroller)
        set_tests_properties(localizationcontroller PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        if(TARGET Qt6::QuickTest)
            qt_add_executable(tst_qmlui
                tests/qmltestmain.cpp
            )
            set_target_properties(tst_qmlui PROPERTIES WIN32_EXECUTABLE FALSE)

            qt_add_qml_module(tst_qmlui
                URI SZHBooksUiTest
                VERSION 1.0
                QML_FILES
                    library/CreateCollectionDialog.qml
                    library/BookActionsDialog.qml
                    library/CollectionActionsDialog.qml
                    library/LibraryBookCover.qml
                    library/LibraryBookDelegate.qml
                    library/LibraryCollectionSidebar.qml
                    library/LibraryImportProgress.qml
                    library/LibrarySyncBar.qml
                    library/SyncCenterDialog.qml
                    shell/ProfileRestoreDialog.qml
                    shell/AppShortcuts.qml
                    ui/SZHButton.qml
                    ui/SZHIconButton.qml
                    ui/SZHMenuButton.qml
                    ui/SZHProgressBar.qml
                    ui/SZHScrollBar.qml
                    ui/SZHSegmentedControl.qml
                    ui/SZHSlider.qml
                    ui/SZHTextField.qml
                    ui/Theme.qml
            )

            target_link_libraries(tst_qmlui
                PRIVATE
                    Qt6::Gui
                    Qt6::Quick
                    Qt6::QuickControls2
                    Qt6::QuickTest
                    Qt6::Test
            )

            target_compile_definitions(tst_qmlui PRIVATE
                QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/qml"
            )

            add_test(NAME qmlui COMMAND tst_qmlui)
            set_tests_properties(qmlui PROPERTIES
                ENVIRONMENT
                    "QT_QUICK_CONTROLS_STYLE=Basic"
                ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            )

            add_test(NAME qmlui_hidpi COMMAND tst_qmlui)
            set_tests_properties(qmlui_hidpi PROPERTIES
                ENVIRONMENT
                    "QT_QUICK_CONTROLS_STYLE=Basic;QT_SCALE_FACTOR=1.5"
                ENVIRONMENT_MODIFICATION
                    "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            )
        endif()
    endif()
endif()

set(CPACK_PACKAGE_NAME "SZHBooks")
set(CPACK_PACKAGE_VENDOR "SZHerio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A calm, local-first desktop reader")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "SZHBooks-${PROJECT_VERSION}-win64")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "SZHBooks")
set(CPACK_GENERATOR "ZIP;IFW")

set(CPACK_IFW_PACKAGE_TITLE "SZHBooks")
set(CPACK_IFW_PACKAGE_PUBLISHER "SZHerio")
set(CPACK_IFW_PRODUCT_URL "https://github.com/SZHerio/SZHBooks")
set(CPACK_IFW_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/szhbooks.ico")
set(CPACK_IFW_PACKAGE_WINDOW_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/branding/szhbooks-icon.png")
set(CPACK_IFW_PACKAGE_WIZARD_STYLE "Modern")
set(CPACK_IFW_PACKAGE_WIZARD_SHOW_PAGE_LIST OFF)
set(CPACK_IFW_PACKAGE_START_MENU_DIRECTORY "SZHBooks")
set(CPACK_IFW_TARGET_DIRECTORY "@ApplicationsDir@/SZHBooks")

if(WIN32 AND NOT CPACK_IFW_ROOT)
    set(CPACK_IFW_ROOT "C:/Qt/Tools/QtInstallerFramework/4.10")
endif()

if(CPACK_IFW_ROOT)
    file(TO_CMAKE_PATH "${CPACK_IFW_ROOT}" CPACK_IFW_ROOT)
endif()

set(CPACK_COMPONENTS_ALL "szhbooks")

include(CPackComponent)
cpack_add_component(szhbooks
    DISPLAY_NAME "SZHBooks"
    DESCRIPTION "SZHBooks desktop reader"
    REQUIRED
)

include(CPackIFW)
cpack_ifw_configure_component(szhbooks
    ESSENTIAL
    FORCED_INSTALLATION
    NAME "com.szhbooks.reader"
    DISPLAY_NAME "SZHBooks"
    DESCRIPTION "A calm, local-first desktop reader"
    VERSION ${PROJECT_VERSION}
    SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/installer/component.qs"
)

include(CPack)
