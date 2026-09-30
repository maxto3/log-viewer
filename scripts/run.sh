#!/usr/bin/env bash
# Runs the built application on Linux.
set -euo pipefail

preset="${1:-linux-gcc-release}"
shift || true
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="$repo_root/build/$preset/bin/log-viewer"

if [ ! -x "$exe" ]; then
    echo "Not built yet: $exe (run scripts/build.sh)" >&2
    exit 1
fi

exec "$exe" "$@"
