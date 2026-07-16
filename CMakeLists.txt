cmake_minimum_required(VERSION 3.16)

project(Leaflit VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Gui Pdf PdfQuick Quick QuickControls2 QuickDialogs2 Xml)

qt_standard_project_setup(REQUIRES 6.8)

set_source_files_properties(ui/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

add_library(leaflit_miniz STATIC
    third_party/miniz/miniz.c
    third_party/miniz/miniz_tdef.c
    third_party/miniz/miniz_tinfl.c
    third_party/miniz/miniz_zip.c
)

include(GenerateExportHeader)
generate_export_header(leaflit_miniz
    BASE_NAME miniz
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h
)

target_include_directories(leaflit_miniz
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz
)

qt_add_executable(appLeaflit
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
    reader/readingdocumentformatter.cpp
    reader/readingdocumentformatter.h
    readercontroller.cpp
    readercontroller.h
    storage/localstatestore.cpp
    storage/localstatestore.h
)

qt_add_qml_module(appLeaflit
    URI Leaflit
    QML_FILES
        Main.qml
        library/LibraryBookDelegate.qml
        library/LibraryView.qml
        reader/ChapterNavigationPopup.qml
        reader/EmptyReaderView.qml
        reader/PageNavigationControl.qml
        reader/PdfDocumentView.qml
        reader/ReaderWorkspace.qml
        reader/TextNavigationControl.qml
        reader/TextDocumentView.qml
        shell/AppHeader.qml
        shell/ReadingSettingsPopup.qml
        shell/ReaderStatusBar.qml
        ui/LeaflitButton.qml
        ui/LeaflitIconButton.qml
        ui/LeaflitProgressBar.qml
        ui/LeaflitScrollBar.qml
        ui/LeaflitSegmentedControl.qml
        ui/LeaflitSlider.qml
        ui/LeaflitSurface.qml
        ui/LeaflitSwitch.qml
        ui/LeaflitTextField.qml
        ui/Theme.qml
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appLeaflit PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appLeafit
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appLeaflit
    PRIVATE
        leaflit_miniz
        Qt6::Gui
        Qt6::Pdf
        Qt6::PdfQuick
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Xml
)

include(GNUInstallDirs)
install(TARGETS appLeaflit
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
                leaflit_miniz
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
    endif()
endif()
