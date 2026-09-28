cmake_minimum_required(VERSION 3.25)

project(log-viewer
    VERSION 1.2.0
    DESCRIPTION "Cross-platform log file viewer (Windows / Linux)"
    HOMEPAGE_URL "https://example.invalid/log-viewer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LOGVIEWER_BUILD_TESTS "Build unit tests" ON)
option(LOGVIEWER_BUILD_BENCH "Build 1M-line performance benchmark" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

qt_standard_project_setup()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(src)

if(LOGVIEWER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS log-viewer
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .)

# --- packaging ---------------------------------------------------------------
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    # Desktop entry, AppStream metadata and the application icon (spec.md
    # REQ-ASSOC-03). The .deb package installs them system wide; the local
    # scripts/register-association.sh script does the same for a user install.
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/log-viewer.desktop.in
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
            RENAME log-viewer.desktop)
    install(FILES ${CMAKE_SOURCE_DIR}/packaging/linux/org.logviewer.LogViewer.metainfo.xml
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
    install(DIRECTORY ${CMAKE_SOURCE_DIR}/packaging/linux/icons/
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons)

    include(${CMAKE_SOURCE_DIR}/packaging/linux/deb/CPackDeb.cmake)
    include(CPack)
endif()
