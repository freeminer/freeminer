#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
MACHINE=$(uname --machine)
builder="$script_dir/appimage-builder-${MACHINE}.AppImage"

if [[ ! -f "$builder" ]]; then
    wget -O "$builder" https://github.com/AppImageCrafters/appimage-builder/releases/download/v1.1.0/appimage-builder-1.1.0-${MACHINE}.AppImage
fi
if [[ ! -x "$builder" ]]; then
    chmod +x "$builder"
fi

cd "$repo_root"
VERSION="$(git describe)" \
TAR_OPTIONS="--overwrite" \
"$builder" --appimage-extract-and-run --recipe "$script_dir/AppImageBuilder.yml"
