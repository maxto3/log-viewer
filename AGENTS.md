# AGENTS.md

Qt 6 Widgets + C++20 desktop log viewer (CMake + Ninja). Windows/MSVC is the
primary, verified target. The Linux path is written to be portable but is
**not built or tested on this machine** (docs/spec.md REQ-PLAT-10, OPEN-07) — do
not claim Linux support was verified.

## Sources of truth

- `docs/spec.md` — requirement spec, single source of truth (Chinese). Behaviour
  changes must be recorded there and in its revision log first.
- `docs/design-doc.md` — design + implementation record (Chinese). §16.2 and
  §16.6 list deviations and known gaps. Its directory tree and test plan predate
  the code (e.g. it shows `src/model/`, `Application`/`JobRunner`,
  `tst_snippets/encoding/watcher`); trust `src/CMakeLists.txt` and
  `tests/CMakeLists.txt` over it.
- `README.md` (English) and `README.zh_CN.md` — user docs; keep both in sync.

## Commands (Windows; all `scripts\*.ps1` require PowerShell 7+)

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
  and `package.ps1`); override with `build.ps1 -QtRoot` or `-DCMAKE_PREFIX_PATH`.
  On this machine downloads need a proxy:
  `.\scripts\install-qt.ps1 -Proxy http://localhost:1081`.
- Plain `cmake --preset ...` commands only work after importing `vcvars64`;
  outside `build.ps1` Ninja cannot find `cl.exe`.

## Tests

- One Qt Test executable per `tests/tst_*.cpp` (14 targets). Adding a test means
  a new file plus an entry in `LOGVIEWER_TESTS` in `tests/CMakeLists.txt`; all
  sources are listed explicitly, there is no glob.
- `test-data/` is git-ignored; tests that need those files are compiled out when
  missing (`EXISTS` guards in `tests/CMakeLists.txt`). Never assert exact counts
  from the live `sslocal.*.log` samples — the deterministic fixture is the
  versioned `tests/data/tracing-sample.log` (500 lines).
- Test targets link `logviewer_ui`, so UI edits can break behaviour suites.
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
  `src/platform/` — the sole existing exception is console attach in
  `src/main.cpp`. Use `QDir`/`QFileInfo`/`QStandardPaths` for paths; never assume
  path separators, case sensitivity, line endings, or local encoding.
- `core`/`highlight`/`platform`/`CliParser` must not depend on `src/ui`.
- Every user-visible string goes through `tr()`; update the translation source
  `src/i18n/logviewer_zh_CN.ts` (compiled to `.qm` next to the exe at build
  time). The UI defaults to English, `--lang zh_CN` previews the Chinese UI.

## Conventions

- Code comments and commit messages in English; commits use `area: summary`
  (e.g. `parser: add windows event text format`).
- Naming: `PascalCase` classes, `m_` members, `camelCase` methods,
  `kPascalCase` constants.
- New log formats require positive and negative samples and both CRLF and LF
  variants.
- Settings live in `%APPDATA%\LogViewer\settings.ini`; tests and manual runs
  share that file, so stale state can explain surprising UI behaviour.
- The app is a Windows GUI-subsystem binary. `--help`/`--version` attach to the
  parent console via `AttachConsole`; check output and exit codes (0/1/2)
  through `run.ps1` or an attached console.
- Version bumps need two edits: `VERSION` in `CMakeLists.txt` (scripts and
  packaging read it) and the hardcoded `setApplicationVersion` in `src/main.cpp`.
