cmake_minimum_required(VERSION 3.16)

project(Leaflit VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS CorePrivate Gui Pdf PdfQuick Quick QuickControls2 QuickDialogs2 Xml)

qt_standard_project_setup(REQUIRES 6.8)

set_source_files_properties(ui/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE
)

qt_add_executable(appLeaflit
    documentloadresult.h
    documentreader.h
    documentreaders.cpp
    documentreaders.h
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
        reader/EmptyReaderView.qml
        reader/PdfDocumentView.qml
        reader/ReaderWorkspace.qml
        reader/TextDocumentView.qml
        shell/AppHeader.qml
        shell/ReadingSettingsPopup.qml
        shell/ReaderStatusBar.qml
        ui/LeaflitButton.qml
        ui/LeaflitIconButton.qml
        ui/LeaflitProgressBar.qml
        ui/LeaflitScrollBar.qml
        ui/LeaflitSlider.qml
        ui/LeaflitSurface.qml
        ui/LeaflitSwitch.qml
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
        Qt6::CorePrivate
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
    endif()
endif()
