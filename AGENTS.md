# AGENTS.md

Qt 6 Widgets + C++20 desktop log viewer (CMake + Ninja), Windows and Linux.
Windows/MSVC (Qt 6.8.3) and Debian forky/sid (GCC 16.2, Qt 6.11.2) are the
verified targets: clean Linux build, 15/15 tests on Wayland and offscreen, CLI /
demo smoke runs and `.deb` generation (contents checked; never installed system
wide — docs/design-doc.md §16.8). Report what you ran; don't extend "verified" to
environments or steps you did not exercise.

## Sources of truth

- `docs/spec.md` — requirement spec, single source of truth (Chinese). Record
  requirement/behaviour changes there with a revision-log entry first, then sync
  `docs/design-doc.md`.
- `docs/design-doc.md` — design + implementation record (Chinese). §16.2 lists
  deviations, §16.5/§16.6 known issues and gaps. Its directory tree and test plan
  predate the code (e.g. it shows `src/model/`, `Application`/`JobRunner`,
  `tst_snippets/encoding/watcher`); trust `src/CMakeLists.txt` and
  `tests/CMakeLists.txt` over it.
- `README.md` (English) and `README.zh_CN.md` — user docs; keep both in sync.

## Commands

Windows (all `scripts\*.ps1` require PowerShell 7+):

- Build: `.\scripts\build.ps1` — locates VS 2022, imports `vcvars64`, then runs
  `cmake --preset windows-msvc-qt6-release` + Ninja. Output:
  `build\windows-msvc-qt6-release\bin\log-viewer.exe`.
- Test: `.\scripts\test.ps1`; one suite: `.\scripts\test.ps1 -Filter tst_formats`.
  `ctest` ships with Visual Studio, not with standalone CMake, and is often not
  on PATH — use the script or a VS developer shell.
- Run: `.\scripts\run.ps1 --demo`, `.\scripts\run.ps1 --lang zh_CN <file>`,
  `.\scripts\run.ps1 -AppArgs '--version'`.
- Package: `.\scripts\package.ps1 -Config Release` →
  `dist\log-viewer-<version>-win64.zip`.
- Qt 6.8.3 lives at `D:/Qt/6.8.3/msvc2022_64` (hardcoded in `CMakePresets.json`
  and `package.ps1`); override with `build.ps1 -QtRoot` or `-DCMAKE_PREFIX_PATH`
  (`package.ps1` also honors `$env:LOGVIEWER_QT_ROOT` to locate `windeployqt`).
  On the Windows dev machine downloads need a proxy:
  `.\scripts\install-qt.ps1 -Proxy http://localhost:1081`.
- Plain `cmake --preset ...` commands only work after importing `vcvars64`;
  outside `build.ps1` Ninja cannot find `cl.exe`.
- `build.ps1`, `test.ps1`, `run.ps1` and `package.ps1` take `-Config Debug` for
  the `windows-msvc-qt6-debug` preset.

Linux (bash; `scripts/*.sh` equivalents, package list in the README):

- The `*.sh` scripts are committed without the exec bit — `./scripts/*.sh` fails
  with "Permission denied" on a fresh checkout; run `chmod +x scripts/*.sh`
  once, or prefix the commands below with `bash`.
- Build: `./scripts/build.sh` (optional preset, default `linux-gcc-release`) →
  `build/linux-gcc-release/bin/log-viewer`.
- Test: `./scripts/test.sh`; one suite:
  `ctest --preset linux-gcc-release -R tst_formats --output-on-failure`.
  `test.sh` adds `QT_QPA_PLATFORM=offscreen` when no display is available.
- Run: `./scripts/run.sh` — the **first argument is the preset**, app options
  come after it (`./scripts/run.sh linux-gcc-release --demo`).
- `.deb`: `./scripts/package-deb.sh`; user MIME association:
  `./scripts/register-association.sh`.
- Needs `build-essential cmake ninja-build qt6-base-dev qt6-base-dev-tools
  qt6-l10n-tools qt6-tools-dev libgl1-mesa-dev`. `qt6-tools-dev` is easy to
  miss: on Debian unstable `qt6-l10n-tools` ships only the lupdate/lrelease
  binaries, while `Qt6LinguistTools` (required by `find_package(Qt6)`) lives in
  `qt6-tools-dev`.
- This box has system Qt 6.11.2 (newer than the Windows 6.8.3);
  `find_package(Qt6 6.5 ...)` accepts both.

## Tests

- One Qt Test executable per `tests/tst_*.cpp` (15 targets). Adding a test means
  a new file plus an entry in `LOGVIEWER_TESTS` in `tests/CMakeLists.txt`; all
  sources are listed explicitly, there is no glob.
- `test-data/` is git-ignored; tests that need those files are compiled out when
  missing (`EXISTS` guards in `tests/CMakeLists.txt`). Never assert exact counts
  from the live `sslocal.*.log` samples — the deterministic fixture is the
  versioned `tests/data/tracing-sample.log` (500 lines).
- Test targets link `logviewer_ui`, so UI edits can break behaviour suites.
- `tst_association` round-trips the real HKCU registry on Windows (test-only
  `.lvtest` extension, cleaned up; skip on Linux). The settings and UI suites
  use temp `.ini` files and never touch the real settings.
- The UI suite runs under KDE Wayland here: window resize and full screen are
  asynchronous (the compositor can override them). Reuse the retry patterns in
  `tst_mainwindow_behavior` (`leaveFullScreen`, the resize loop) for new
  window-management cases, and pin the window size when layout math must not
  depend on the host screen (`offscreen` defaults to 800x800).
- `LOGVIEWER_BUILD_BENCH` is declared in `CMakeLists.txt` but wired to no target
  (the 1M-line benchmark was never written); `LOGVIEWER_BUILD_TESTS` is ON by
  default.
- No CI, linter or formatter config: compiler warnings (`/W4`, `-Wall -Wextra`,
  plus `-Wpedantic` for the libraries) are the only static gate.
- Layout self-check: set `LOGVIEWER_DUMP_LAYOUT=<file>` (optionally
  `LOGVIEWER_DUMP_LAYOUT_DELAY=<ms>`, default 1200) to dump widget geometries
  and exit.

## Architecture rules

- `src/app` (CLI, settings, theme, translations), `src/core` (parsers, line
  index, document, model, matcher, watcher), `src/highlight`, `src/platform`
  (only place platform differences may live), `src/ui`. Static libs are
  `logviewer_core` and `logviewer_ui`; every file must be listed in
  `src/CMakeLists.txt`.
- Platform conditionals (`#ifdef _WIN32`, `Q_OS_*`) belong only in
  `src/platform/`. The only exceptions: console attach in `src/main.cpp` and the
  non-Windows system translation path in `src/app/TranslationManager.cpp`. Use
  `QDir`/`QFileInfo`/`QStandardPaths` for paths; never assume path separators,
  case sensitivity, line endings, or local encoding.
- `core`/`highlight`/`platform`/`CliParser` must not depend on `src/ui`.
- Every user-visible string goes through `tr()`; add new strings to
  `src/i18n/logviewer_zh_CN.ts` by hand or with `lupdate` — the build only runs
  `lrelease` (`qt_add_lrelease`) and copies `.qm` files to `translations/` next
  to the exe, so a missing `.ts` entry silently falls back to English. The UI
  defaults to English, `--lang zh_CN` previews Chinese.

## Conventions

- Code comments and commit messages in English; commits use `area: summary`
  (e.g. `parser: add windows event text format`).
- Naming: `PascalCase` classes, `m_` members, `camelCase` methods,
  `kPascalCase` constants.
- New log formats: implement `ILogFormat` in `src/core/formats/`, register it in
  `LogFormatRegistry.cpp` (the generic fallback must stay last), and add
  deterministic cases to the format suites — positive and negative samples in
  both CRLF and LF variants (REQ-MAINT-05). `--format list` prints the ids.
- The app reads and writes `%APPDATA%\LogViewer\settings.ini` (Windows) or
  `~/.config/LogViewer/settings.ini` (Linux); manual runs share that file, so
  stale state can explain surprising UI behaviour.
- On Windows the app is a GUI-subsystem binary, so `--help`/`--version` attach
  to the parent console via `AttachConsole`. `--help`, `--version` and the
  association options run console-only, before any window is created; check
  output and exit codes (0/1/2) through `run.ps1` or an attached console.
- Version bumps need two edits: `VERSION` in `CMakeLists.txt` (scripts and
  packaging read it) and the hardcoded `setApplicationVersion` in `src/main.cpp`.
