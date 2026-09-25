#!/usr/bin/env bash
set -xeuo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/../.." && pwd)"
image="freeminer-appimage-builder"

docker build -f "$script_dir/Dockerfile" -t "$image" "$script_dir"
docker_socket_group=""
if [[ -S /var/run/docker.sock ]]; then
    docker_socket_group="$(stat --format '%g' /var/run/docker.sock)"
fi

docker_group_args=()
if [[ -n "$docker_socket_group" ]]; then
    docker_group_args+=(--group-add "$docker_socket_group")
fi

docker run --rm \
    --user "$(id -u):$(id -g)" \
    "${docker_group_args[@]}" \
    -e HOME=/tmp \
    -e USER=appimage \
    -e LOGNAME=appimage \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -e DISPLAY \
    -v "$repo_root:$repo_root" \
    --workdir "$repo_root" \
    "$image" "$repo_root/build_tools/appimage/appimage.sh"

[ -f deploy.sh ] && source deploy.sh
