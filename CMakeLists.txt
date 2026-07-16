cmake_minimum_required(VERSION 3.16)

project(SZHBooks VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Pdf PdfQuick Quick QuickControls2 QuickDialogs2 Xml)

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

qt_add_executable(appSZHBooks
    archive/ziparchivereader.cpp
    archive/ziparchivereader.h
    documentloadresult.h
    documentreader.h
    documentreaders.cpp
    documentreaders.h
    library/librarybook.h
    library/librarymodel.cpp
    library/librarymodel.h
    localdocumentloader.cpp
    localdocumentloader.h
    main.cpp
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
    storage/readingannotationstore.cpp
    storage/readingannotationstore.h
)

qt_add_qml_module(appSZHBooks
    URI SZHBooks
    QML_FILES
        Main.qml
        library/LibraryBookDelegate.qml
        library/LibraryView.qml
        reader/ChapterNavigationPopup.qml
        reader/AnnotationsPopup.qml
        reader/EmptyReaderView.qml
        reader/PageNavigationControl.qml
        reader/PdfDocumentView.qml
        reader/ReaderWorkspace.qml
        reader/SearchPopup.qml
        reader/TextNavigationControl.qml
        reader/TextDocumentView.qml
        shell/AppHeader.qml
        shell/ReaderToolsPopup.qml
        shell/ReadingSettingsPopup.qml
        shell/ReaderStatusBar.qml
        ui/SZHBrandLogo.qml
        ui/SZHButton.qml
        ui/SZHIconButton.qml
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
        assets/branding/szhbooks-logo-sheet.png
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

target_link_libraries(appSZHBooks
    PRIVATE
        szhbooks_miniz
        Qt6::Gui
        Qt6::Pdf
        Qt6::PdfQuick
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Xml
)

include(GNUInstallDirs)
install(TARGETS appSZHBooks
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 QUIET COMPONENTS Test)

    if(TARGET Qt6::Test)
        qt_add_executable(tst_localstatestore
            library/librarybook.h
            library/librarymodel.cpp
            library/librarymodel.h
            storage/documentstoragekey.cpp
            storage/documentstoragekey.h
            storage/localstatestore.cpp
            storage/localstatestore.h
            tests/localstatestoretest.cpp
        )

        target_link_libraries(tst_localstatestore
            PRIVATE
                Qt6::Core
                Qt6::Test
        )

        add_test(NAME localstatestore COMMAND tst_localstatestore)
        set_tests_properties(localstatestore PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
        )

        qt_add_executable(tst_documentreaders
            archive/ziparchivereader.cpp
            archive/ziparchivereader.h
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
    endif()
endif()
