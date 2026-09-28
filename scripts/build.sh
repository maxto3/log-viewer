#!/usr/bin/env bash
# Configures and builds the project on Linux (GCC/Clang + system Qt 6).
#
# NOTE: this path has not been verified on this development machine (spec.md
# REQ-PLAT-10); it is provided for the native Debian verification the user plans.
set -euo pipefail

preset="${1:-linux-gcc-release}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

echo "Configuring ($preset) ..."
cmake --preset "$preset"
echo "Building ..."
cmake --build --preset "$preset"

echo "Built: $repo_root/build/$preset/bin/log-viewer"
