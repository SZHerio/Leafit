if(NOT DEFINED SZHBOOKS_INSTALL_ROOT OR SZHBOOKS_INSTALL_ROOT STREQUAL "")
    message(FATAL_ERROR "SZHBOOKS_INSTALL_ROOT is required")
endif()

file(TO_CMAKE_PATH "${SZHBOOKS_INSTALL_ROOT}" install_root)

set(required_files
    "SZHBooks.exe"
    "Qt6Core.dll"
    "Qt6Gui.dll"
    "Qt6Qml.dll"
    "Qt6Quick.dll"
    "plugins/platforms/qwindows.dll"
    "plugins/sqldrivers/qsqlite.dll"
)

foreach(relative_path IN LISTS required_files)
    if(NOT EXISTS "${install_root}/${relative_path}")
        message(FATAL_ERROR "Installed package is missing ${relative_path}")
    endif()
endforeach()

file(SIZE "${install_root}/SZHBooks.exe" executable_size)
if(executable_size LESS 100000)
    message(FATAL_ERROR "Installed SZHBooks.exe is unexpectedly small")
endif()

message(STATUS "Verified SZHBooks install tree at ${install_root}")
