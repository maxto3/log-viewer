#!/usr/bin/env bash
# Registers (or removes) the desktop / MIME association on Linux.
#
#   scripts/register-association.sh            # register
#   scripts/register-association.sh --unregister
#
# A system installation via the .deb package does this automatically; the script
# is meant for a local (make install) or portable installation.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
desktop_source="$repo_root/build/linux-gcc-release/log-viewer.desktop"
installed_desktop="$HOME/.local/share/applications/log-viewer.desktop"
exe="$repo_root/build/linux-gcc-release/bin/log-viewer"

unregister=false
if [ "${1:-}" = "--unregister" ]; then
    unregister=true
fi

if $unregister; then
    rm -f "$installed_desktop"
    update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
    echo "Association removed."
    exit 0
fi

if [ ! -f "$desktop_source" ]; then
    echo "Desktop file not found: $desktop_source (build first or install the .deb)" >&2
    exit 1
fi

mkdir -p "$HOME/.local/share/applications"
sed "s|^Exec=.*|Exec=\"$exe\" %F|" "$desktop_source" > "$installed_desktop"
update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true

if command -v xdg-mime >/dev/null 2>&1; then
    # xdg-mime does not create the config directory itself (it fails with a
    # touch/awk error on a fresh account), so make sure it exists.
    mkdir -p "${XDG_CONFIG_HOME:-$HOME/.config}"
    xdg-mime default log-viewer.desktop text/x-log
    xdg-mime default log-viewer.desktop application/x-ndjson
    echo "Registered log-viewer.desktop as the handler for text/x-log."
fi

echo "Done. Undo with: $0 --unregister"
