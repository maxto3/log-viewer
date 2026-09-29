set(LOGVIEWER_CORE_SOURCES
    app/CliParser.cpp
    app/CliParser.h
    app/SettingsStore.cpp
    app/SettingsStore.h
    app/ThemeManager.cpp
    app/ThemeManager.h
    app/TranslationManager.cpp
    app/TranslationManager.h

    platform/EncodingBackend.cpp
    platform/EncodingBackend.h
    platform/PlatformInfo.cpp
    platform/PlatformInfo.h

    core/ColumnKind.h
    core/DurationFormat.cpp
    core/DurationFormat.h
    core/FilterSpec.h
    core/IEntryProvider.h
    core/LogDocument.cpp
    core/LogDocument.h
    core/LogEntry.h
    core/LogFormat.cpp
    core/LogFormat.h
    core/LogFormatRegistry.cpp
    core/LogFormatRegistry.h
    core/LogLevel.cpp
    core/LogLevel.h
    core/LogSource.cpp
    core/LogSource.h
    core/LogTableModel.cpp
    core/LogTableModel.h
    core/LineIndex.cpp
    core/LineIndex.h
    core/LogWatcher.cpp
    core/LogWatcher.h
    core/Matcher.cpp
    core/Matcher.h
    core/TimestampParser.cpp
    core/TimestampParser.h
    core/formats/ApplicationFormats.cpp
    core/formats/ApplicationFormats.h
    core/formats/GenericFormat.cpp
    core/formats/GenericFormat.h
    core/formats/StructuredFormats.cpp
    core/formats/StructuredFormats.h
    core/formats/SyslogFormats.cpp
    core/formats/SyslogFormats.h
    core/formats/TracingFormat.cpp
    core/formats/TracingFormat.h
    core/formats/WindowsFormats.cpp
    core/formats/WindowsFormats.h

    highlight/HighlightTheme.cpp
    highlight/HighlightTheme.h
    highlight/LevelPalette.cpp
    highlight/LevelPalette.h
    highlight/MessageTextHighlighter.cpp
    highlight/MessageTextHighlighter.h
    highlight/SnippetTokenizer.cpp
    highlight/SnippetTokenizer.h
)

# Widgets independent library: everything below has no idea about the GUI.
qt_add_library(logviewer_core STATIC ${LOGVIEWER_CORE_SOURCES})
target_include_directories(logviewer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logviewer_core PUBLIC Qt6::Core Qt6::Gui Qt6::Widgets)

set(LOGVIEWER_UI_SOURCES
    ui/AboutDialog.cpp
    ui/AboutDialog.h
    ui/DemoData.cpp
    ui/DemoData.h
    ui/DetailPane.cpp
    ui/DetailPane.h
    ui/FilterPanel.cpp
    ui/FilterPanel.h
    ui/LogHeaderView.cpp
    ui/LogHeaderView.h
    ui/LogItemDelegate.cpp
    ui/LogItemDelegate.h
    ui/LogTableView.cpp
    ui/LogTableView.h
    ui/MainWindow.cpp
    ui/MainWindow.h
)

# Widgets library, used by the application as well as by the UI behaviour tests.
qt_add_library(logviewer_ui STATIC ${LOGVIEWER_UI_SOURCES})
target_include_directories(logviewer_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(logviewer_ui PUBLIC logviewer_core Qt6::Widgets)

qt_add_executable(log-viewer
    main.cpp
    "${CMAKE_SOURCE_DIR}/resources/resources.qrc")
set_target_properties(log-viewer PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(log-viewer PRIVATE logviewer_ui)

# Windows: the executable carries the application icon for Explorer, the
# taskbar and Alt-Tab (REQ-VIS-11).  The .ico is rendered from the Linux
# desktop icon artwork; rc.exe resolves resource paths relative to its working
# directory, so the absolute path is substituted here.
if(WIN32)
    file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}/packaging/windows/log-viewer.ico"
         LOGVIEWER_WINDOWS_ICON)
    configure_file("${CMAKE_SOURCE_DIR}/packaging/windows/log-viewer.rc.in"
                   "${CMAKE_CURRENT_BINARY_DIR}/log-viewer.rc" @ONLY)
    target_sources(log-viewer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/log-viewer.rc")
endif()

function(logviewer_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

logviewer_enable_warnings(logviewer_core)
logviewer_enable_warnings(logviewer_ui)
logviewer_enable_warnings(log-viewer)

# Translations: logviewer_<locale>.qm is produced next to the executable so the
# application can load it at runtime (see TranslationManager).
qt_add_lrelease(log-viewer
    TS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/i18n/logviewer_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE logviewer_qm_files
)
add_custom_command(TARGET log-viewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:log-viewer>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${logviewer_qm_files}
            "$<TARGET_FILE_DIR:log-viewer>/translations/"
    COMMENT "Copying translations next to the executable"
)

# System installation layout on Linux (design-doc §9.2).
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(FILES ${logviewer_qm_files}
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/log-viewer/translations)
endif()

# Windows: copy the Qt runtime next to the executable so the build tree can be
# started directly (scripts/package.ps1 builds the redistributable folder).
if(WIN32)
    find_program(LOGVIEWER_WINDEPLOYQT windeployqt HINTS "${QT6_INSTALL_BINS}")
    if(LOGVIEWER_WINDEPLOYQT)
        add_custom_command(TARGET log-viewer POST_BUILD
            COMMAND "${LOGVIEWER_WINDEPLOYQT}" --no-opengl-sw --no-system-d3d-compiler
                    "$<TARGET_FILE:log-viewer>"
            COMMENT "Deploying the Qt runtime next to the executable"
            VERBATIM)
    endif()
endif()
