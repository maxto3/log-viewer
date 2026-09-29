find_package(Qt6 REQUIRED COMPONENTS Test)

# Locate the Qt runtime directory (Windows: <prefix>/bin, Linux: <prefix>/lib or
# the multiarch lib dir) so the test executables can be started without a full
# deployment step.
set(LOGVIEWER_QT_RUNTIME_DIR "")
set(_qt_prefixes ${CMAKE_PREFIX_PATH})
get_filename_component(_qt_prefix_from_package "${Qt6Core_DIR}/../../.." ABSOLUTE)
list(APPEND _qt_prefixes "${_qt_prefix_from_package}")
foreach(_prefix IN LISTS _qt_prefixes)
    if(WIN32 AND EXISTS "${_prefix}/bin/Qt6Core.dll")
        set(LOGVIEWER_QT_RUNTIME_DIR "${_prefix}/bin")
        break()
    endif()
endforeach()

set(LOGVIEWER_TESTS
    tst_cli.cpp
    tst_loglevel.cpp
    tst_timestamp.cpp
    tst_formats.cpp
    tst_lineindex.cpp
    tst_settings.cpp
    tst_monitor.cpp
    tst_matcher.cpp
    tst_filter.cpp
    tst_highlight.cpp
    tst_formats_extra.cpp
    tst_document.cpp
    tst_mainwindow_behavior.cpp
    tst_duration.cpp
    tst_association.cpp
)

foreach(test_source IN LISTS LOGVIEWER_TESTS)
    get_filename_component(test_name ${test_source} NAME_WE)
    qt_add_executable(${test_name} ${test_source})
    set_target_properties(${test_name} PROPERTIES WIN32_EXECUTABLE FALSE)
    target_link_libraries(${test_name} PRIVATE logviewer_ui Qt6::Test)
    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${test_name} COMMAND ${test_name})

    if(LOGVIEWER_QT_RUNTIME_DIR)
        # Qt6Test is not part of the application deployment, so copy it next to
        # the test binaries and extend the loader path for CTest.
        if(WIN32 AND EXISTS "${LOGVIEWER_QT_RUNTIME_DIR}/Qt6Test.dll")
            add_custom_command(TARGET ${test_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${LOGVIEWER_QT_RUNTIME_DIR}/Qt6Test.dll"
                        "$<TARGET_FILE_DIR:${test_name}>"
                COMMENT "Copying Qt6Test.dll next to the test binaries")
        endif()
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${LOGVIEWER_QT_RUNTIME_DIR}")
    endif()
endforeach()

# Real log sample used by the format tests (regression data, spec.md REQ-MAINT-02).
# The frozen slice lives in the source tree (deterministic assertions), the live
# file is only used for structural assertions because it keeps growing.
set(LOGVIEWER_FROZEN_SAMPLE ${CMAKE_CURRENT_SOURCE_DIR}/data/tracing-sample.log)
if(EXISTS ${LOGVIEWER_FROZEN_SAMPLE})
    target_compile_definitions(tst_formats PRIVATE
        LOGVIEWER_FROZEN_SAMPLE="${LOGVIEWER_FROZEN_SAMPLE}")
endif()

if(EXISTS ${LOGVIEWER_FROZEN_SAMPLE})
    target_compile_definitions(tst_mainwindow_behavior PRIVATE
        LOGVIEWER_FROZEN_SAMPLE="${LOGVIEWER_FROZEN_SAMPLE}")
endif()

set(LOGVIEWER_SAMPLE_LOG ${CMAKE_CURRENT_SOURCE_DIR}/../test-data/sslocal.2026-09-28.log)
if(EXISTS ${LOGVIEWER_SAMPLE_LOG})
    target_compile_definitions(tst_formats PRIVATE
        LOGVIEWER_SAMPLE_LOG="${LOGVIEWER_SAMPLE_LOG}")
endif()
