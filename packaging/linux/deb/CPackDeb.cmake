# CPack configuration for the Debian package (design-doc §9.2).
#
# Included by the top level CMakeLists.txt on Linux. The package installs the
# binary, the desktop entry, the AppStream metadata and the hicolor icon; the
# Qt dependencies are listed explicitly so dpkg can pull them in.

set(CPACK_PACKAGE_NAME "log-viewer")
set(CPACK_PACKAGE_VENDOR "log-viewer project")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform log file viewer")
set(CPACK_PACKAGE_DESCRIPTION "Table based log reader with level, time and keyword filtering, VSCode style syntax highlighting for embedded JSON/XML/YAML snippets and live monitoring (tail -f).")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "log-viewer maintainers <log-viewer@example.invalid>")

set(CPACK_GENERATOR "DEB;TGZ")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6, libqt6gui6, libqt6core6")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)

set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${PROJECT_VERSION}_amd64")
