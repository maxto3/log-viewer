# Log Viewer

A fast, cross-platform desktop log file viewer (Windows and Linux) built with
**Qt 6 Widgets and C++20**. It reads text log files into a table, lets you search
and filter them, highlights embedded JSON/XML/YAML snippets with the Visual
Studio Code colour scheme, and can follow a file live like `tail -f`.

![Details pane and filters](screenshots/02-reallog-chinese-preview.png)

## Features

| Area | What you get |
| --- | --- |
| Reading | Table with automatically detected columns (Line, Time, Level, Thread, Target, …); the Line column shows the source line, or the record number for the Windows event XML/TSV exports; alternating row colours and cell separator lines; two-line rows with `…` truncation; Enter expands a row |
| Log formats | Rust `tracing`, syslog (RFC 3164 / 5424), `journalctl` (short and JSON), JSON lines, logfmt, CSV/TSV, Python `logging`, Serilog, log4j/Logback, Windows event log exports (Event Viewer text/TAB, XML and `Format-List` blocks), IIS W3C, plus a generic fallback for any text log |
| Find | Highlights matches (default green background / black text) without hiding rows; whole word, wildcard (`*` `?`) or regular expression; case sensitivity switch; `F3` / `Shift+F3` navigation with a match counter |
| Filter | Hides rows by level, time range (absolute instants) or keyword; the keyword filter offers an **Invert** option that keeps only the rows *without* a match; the level check box list is built from the levels that occur in the loaded file, so it follows each log; conditions combine with AND; status bar shows `shown / total lines` |
| Syntax highlighting | JSON, XML and YAML fragments inside messages get the VSCode Dark+ / Light+ palette; details pane follows the theme |
| Details pane | Optional pane (right or bottom) with bold field names and the complete, never truncated message. While it is hidden the table switches to complete rows: every entry is shown in full, nothing is truncated; double click any cell to copy its full text |
| Live monitoring | `Ctrl+M` follows a single file like `tail -f`: appended lines appear automatically, the view follows the tail, rotation/truncation reloads the document |
| Multiple files | Open or drop several files: same-format files are merged by timestamp with an additional File column; different formats are rejected with an explanation |
| Encodings | UTF-8 (with or without BOM), UTF-16, and legacy encodings auto-detected (GB18030/GBK, Big5, Shift_JIS, CP1252) and shown as UTF-8 |
| Console logs | ANSI escape sequences are stripped (cursor/title noise) and terminal colours — 8/16-colour, 256-colour and truecolour, plus bold/italic/underline — are rendered in the table and the details pane; LF, CRLF and CR-only line endings are indexed correctly (for example systemd `boot.log`) |
| Languages | Complete English and Simplified Chinese user interface, switchable at runtime |
| Themes | Light, Dark or follow the system; separate syntax highlighting theme setting |
| View | Collapse the search & filter panel to its title row to give the table more room; full screen (`F11`, leave with `Esc`) automatically collapses that panel |
| Status bar | The bottom left corner shows how long the last file open took (`Loaded in 0.35 s`; the value adapts to seconds / minutes / hours); the right side shows the file name, format, encoding, line count and monitoring state |
| System logs (Linux) | Files without read permission keep a persistent red alert and offer **Open as Administrator…**: the file is read through the system authentication helper (`pkexec`/polkit) into a private read-only snapshot; monitoring is disabled for snapshots and Refresh re-reads the original file. Without `pkexec` the alert stays and explains how to grant access manually |

## Requirements

* Windows 10/11 x64 (primary) or Linux x64 with glibc ≥ 2.28
* Qt 6.8.x (LGPLv3) — installed by the helper script on Windows
* CMake ≥ 3.25, Ninja, a C++20 compiler (MSVC 2022 on Windows, GCC ≥ 13 / Clang ≥ 16 on Linux)

## Build and run (Windows)

```powershell
git clone <this repository>
cd log-viewer

# 1) one time: download Qt 6.8.3 LTS (needs ~2 GB, supports an HTTP proxy)
.\scripts\install-qt.ps1 -Proxy http://localhost:1081     # omit -Proxy when not needed

# 2) build (locates Visual Studio, imports vcvars64, runs CMake + Ninja)
.\scripts\build.ps1

# 3) run
.\scripts\run.ps1                                                     # empty window
.\scripts\run.ps1 --demo                                              # preview with sample data
.\scripts\run.ps1 --lang zh_CN .\test-data\sslocal.2026-09-28.log     # Chinese UI with a real log
.\scripts\run.ps1 -AppArgs '--version'

# 4) tests
.\scripts\test.ps1

# 5) portable package (executable + Qt runtime + translations, one zip)
.\scripts\package.ps1 -Config Release                    # -> dist\log-viewer-<version>-win64.zip

# 6) optional: associate .log files with Log Viewer (current user only, reversible)
.\scripts\register-association.ps1                    # from a checkout
.\scripts\register-association.ps1 -Unregister        # undo, restores the old handler
# after unpacking the release zip (e.g. into C:\log-viewer) use the executable
# itself or the script shipped next to it — both register that very copy:
#   C:\log-viewer\log-viewer.exe --register-association
#   C:\log-viewer\log-viewer.exe --unregister-association
```

The build output lives in `build\windows-msvc-qt6-release\bin\log-viewer.exe`.
That folder is already self contained (`windeployqt` ran) — `package.ps1` deploys
the Qt runtime into a staging folder and compresses it with the executable, the
plugins, the translations and the documentation into a single
`dist\log-viewer-<version>-win64.zip`. `dist\` holds only that archive; it
extracts to a top-level `log-viewer-<version>\` folder. The version comes from
`CMakeLists.txt`.

Manual build (any shell with CMake and Ninja on `PATH`):

```powershell
cmake --preset windows-msvc-qt6-release
cmake --build --preset windows-msvc-qt6-release
ctest --preset windows-msvc-qt6-release --output-on-failure
```

## Build and run (Linux)

Verified on Debian forky/sid with GCC 16.2 and Qt 6.11.2 (KDE Wayland and the
offscreen platform): build, 17/17 tests, CLI, demo/real-log smoke runs and `.deb`
generation; the `.deb` contents were inspected but not installed system wide.

Root-owned logs (for example `/var/log/auth.log`, mode `640 root:adm`) can be
opened read-only through the system authentication helper: the status bar offers
**Open as Administrator…**, the content is copied into a private snapshot and
the snapshot is deleted when the document is closed. Monitoring is disabled for
such snapshots; Refresh asks for authorization again and re-reads the original
file.

```bash
sudo apt install build-essential cmake ninja-build \
                 qt6-base-dev qt6-base-dev-tools qt6-l10n-tools qt6-tools-dev \
                 libgl1-mesa-dev

./scripts/build.sh              # cmake --preset linux-gcc-release + build
./scripts/test.sh               # ctest (offscreen when no display)
./scripts/run.sh linux-gcc-release --demo   # first argument is the preset

# Debian package and desktop integration
./scripts/package-deb.sh        # -> build/linux-gcc-release/log-viewer_*_amd64.deb
sudo dpkg -i build/linux-gcc-release/log-viewer_*_amd64.deb
# or for a local installation:
./scripts/register-association.sh
```

## Command line

```
log-viewer [options] [files...]

  -h, --help             Show help and exit
  -v, --version          Show version and build information
      --lang <en|zh_CN>  Override the interface language (not stored)
      --format <id>      Force a log format ('auto' to detect, 'list' to print)
      --monitor          Enable live monitoring after opening one file
      --demo             Load built-in demo data (UI preview only)
      --register-association
                         Associate .log files with this executable
                         (current user only; undo with the option below)
      --unregister-association
                         Remove the .log association and restore the
                         previous one
      --force            Replace an existing .log association without warning
                         (--register-association only)
```

Exit codes: `0` success, `1` file error (also a failed registration), `2` usage error.

## Keyboard and mouse

| Input | Action |
| --- | --- |
| Click a row | Show/update the details pane (when *Show Details Pane* is on) |
| **Double click a cell** | Copy the complete cell text to the clipboard |
| `Enter` / `Space` | Expand or collapse the selected row |
| `Ctrl+C` | Copy the selection |
| `F3` / `Shift+F3` | Next / previous Find match |
| `Ctrl+F` / `Ctrl+G` | Focus the Find / Filter box |
| `Enter` in the Find / Filter box | Apply the pattern (Find jumps to the first match, Filter hides non-matching rows); typing alone never refreshes the table |
| `Ctrl+O` / `F5` / `Ctrl+W` / `Ctrl+Q` | Open / refresh / close / quit |
| `Ctrl+M` | Toggle live monitoring |
| `Ctrl+E` | Export the filtered rows (CSV or text) |
| `F11` | Toggle full screen (*Settings ▸ Full Screen*); entering full screen collapses the search & filter panel |
| `Esc` | Leave full screen (the search & filter panel returns to its previous state) |
| `▾` / `▸` in the "Search & Filter" title | Collapse / expand the search & filter panel (only the input rows are hidden; active conditions keep working) |
| `Ctrl` + wheel | Temporary font zoom |
| Drop log files onto the window | Open them (same-format files are merged); folders are ignored |
| Right click | Copy cell / row / message, auto-fit columns |

## Menus

* **File** — Open, Refresh, Close, Monitor (checkbox), Export Filtered Results,
  Recent Files, Exit
* **Settings**
  * **Font** — interface font, table font, header font, reset to defaults
  * **Language** — English / 简体中文 (applies immediately)
  * **Details Pane** — *Show Details Pane*: checked = pane always visible
    (first entry selected on load), unchecked = pane never shown; **Layout**
    submenu selects the pane position (right or bottom)
  * **Appearance** — theme (light/dark/follow system), syntax highlighting theme
    (follow theme / VSCode Dark+ / Light+), highlight colour, reset all settings
  * **File Association** — *Associate .log Files*: checkable, registers the
    running executable for the current user (`HKCU\Software\Classes`) so that
    double-clicking a `.log` file opens it in Log Viewer; unchecking removes the
    association and restores the previous handler. The check mark is read back
    from the registry, so after moving the program folder it shows unchecked
    until you tick it again (nothing else has to be edited). Only `.log` is
    touched — if Windows already pins the extension through
    *Open with ▸ Always use this app* (`UserChoice`), confirm it once there; the
    tool never rewrites that protected key
  * **Full Screen** — checkable, `F11`: collapses the search & filter panel and
    shows the window full screen; `Esc` leaves full screen and restores the panel
* **Columns** — top level menu between Settings and About: one checkable item per
  column of the loaded document (unchecking hides that column, the choice is
  remembered per document); the Line column is always shown; *Show All Columns*
  restores everything
* **About** — top level menu entry next to File and Settings: version, build information, licences

## Settings storage

| Platform | Location |
| --- | --- |
| Windows | `%APPDATA%\LogViewer\settings.ini` |
| Linux | `$XDG_CONFIG_HOME/LogViewer/settings.ini` (usually `~/.config/LogViewer/`) |

Settings include fonts, language, layout, theme, highlight colours, the maximum
number of lines per file, continuation-line merging, recent files and the column
widths of recent documents. `Settings ▸ Appearance ▸ Reset All Settings` restores
the defaults.

## Documentation

* `docs/spec.md` — requirement specification (the single source of truth, Chinese)
* `docs/design-doc.md` — technical design and implementation record (Chinese)
* `screenshots/` — curated UI evidence (index: `screenshots/README.md`)

## Project layout

```
src/app/        settings, theme, translations, command line
src/core/       line index, entry providers, parsers, model, matcher, watcher
src/highlight/  VSCode palettes, JSON/XML/YAML tokenizer, message highlighter
src/platform/   platform specific code (encodings, fonts, paths)
src/ui/         main window, filter panel, table view, details pane, dialogs
tests/          Qt Test suites (14 targets) and the frozen log fixture (tests/data/)
test-data/      local sample logs for manual testing (git-ignored, not versioned)
scripts/        build, test, run, package and file-association scripts
packaging/      Linux desktop entry, AppStream metadata, icon, CPack DEB
```

## Licence

MIT. Qt 6 is used under the terms of the GNU Lesser General Public License v3.
