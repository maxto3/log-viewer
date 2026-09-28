#!/usr/bin/env bash
# Builds a Debian package (.deb) with CPack (design-doc §9.2).
# NOTE: not verified on the Windows development machine (REQ-PLAT-10).
set -euo pipefail

preset="${1:-linux-gcc-release}"
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

cmake --preset "$preset"
cmake --build --preset "$preset"
( cd "build/$preset" && cpack -G DEB )

echo "Package(s):"
ls -1 "build/$preset"/*.deb 2>/dev/null || true
