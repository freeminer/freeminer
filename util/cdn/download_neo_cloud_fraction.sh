#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"

dataset="${NEO_CLOUD_DATASET:-MODAL2_M_CLD_FR}"
year="${NEO_CLOUD_YEAR:-2024}"
cdn_earth_dir="${1:-$repo_dir/cdn/earth}"

base_url="https://neo.gsfc.nasa.gov/archive/gs/${dataset}"
work_dir="$cdn_earth_dir/neo_cloud_${dataset}_${year}"
raw_dir="$work_dir/raw"

usage() {
	echo "Usage: $0 [cdn-earth-dir]"
	echo
	echo "Downloads NASA NEO monthly cloud fraction grayscale PNGs."
	echo "Default CDN directory: $repo_dir/cdn/earth"
	echo "Dataset: NEO_CLOUD_DATASET=$dataset"
	echo "Year:    NEO_CLOUD_YEAR=$year"
	echo
	echo "Output expected by Freeminer:"
	echo "  <cdn-earth-dir>/earth_cloud_01.png .. earth_cloud_12.png"
	echo "  <cdn-earth-dir>/earth_cloud.png if ImageMagick is installed"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

mkdir -p "$cdn_earth_dir" "$work_dir" "$raw_dir"

echo "Downloading NASA NEO cloud fraction:"
echo "  dataset: $dataset"
echo "  year:    $year"

monthly_outputs=()
for month in $(seq -w 1 12); do
	src_url="${base_url}/${dataset}_${year}-${month}.PNG"
	raw_file="$raw_dir/${dataset}_${year}-${month}.PNG"
	dst="$cdn_earth_dir/earth_cloud_${month}.png"

	echo "  $src_url"
	curl -L --fail --continue-at - --output "$raw_file" "$src_url"
	cp "$raw_file" "$dst"
	monthly_outputs+=("$dst")
	echo "    -> $dst"
done

annual="$cdn_earth_dir/earth_cloud.png"
if command -v magick >/dev/null 2>&1; then
	magick "${monthly_outputs[@]}" -evaluate-sequence mean "$annual"
	echo "  $annual"
elif command -v convert >/dev/null 2>&1; then
	convert "${monthly_outputs[@]}" -evaluate-sequence mean "$annual"
	echo "  $annual"
else
	echo
	echo "ImageMagick not found; annual earth_cloud.png was not created."
	echo "Monthly files are enough when weather is enabled."
fi

echo
echo "Done."
