#!/usr/bin/env bash
# Runs the unit tests on Linux.
set -euo pipefail

preset="${1:-linux-gcc-release}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Headless test runs: use the offscreen platform plugin when no display exists.
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    export QT_QPA_PLATFORM=offscreen
fi

ctest --preset "$preset" --output-on-failure
