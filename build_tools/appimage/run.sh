#!/usr/bin/env bash
set -xeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
image="freeminer-appimage-builder"

docker build -f "$script_dir/Dockerfile" -t "$image" "$script_dir"
docker run --rm \
    --user "$(id -u):$(id -g)" \
    -e HOME=/tmp \
    -v "$repo_root:/workspace" \
    "$image"

[ -f deploy.sh ] && source deploy.sh
