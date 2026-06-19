#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"

resolution="${WORLDCLIM_RESOLUTION:-10m}"
cdn_earth_dir="${1:-$repo_dir/cdn/earth}"

url="https://geodata.ucdavis.edu/climate/worldclim/2_1/base/wc2.1_${resolution}_tavg.zip"
work_dir="$cdn_earth_dir/worldclim_tavg_${resolution}"
zip_file="$work_dir/wc2.1_${resolution}_tavg.zip"
tif_dir="$work_dir/tif"

usage() {
	echo "Usage: $0 [cdn-earth-dir]"
	echo
	echo "Downloads WorldClim monthly average temperature and prepares CDN files."
	echo "Default CDN directory: $repo_dir/cdn/earth"
	echo "Resolution: WORLDCLIM_RESOLUTION=$resolution"
	echo
	echo "Output expected by Freeminer:"
	echo "  <cdn-earth-dir>/earth_tavg_01.png .. earth_tavg_12.png"
	echo
	echo "Optional tools:"
	echo "  gdal_translate  converts GeoTIFF -> scaled grayscale PNG"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

mkdir -p "$cdn_earth_dir" "$work_dir" "$tif_dir"

echo "Downloading:"
echo "  $url"
echo "to:"
echo "  $zip_file"

curl -L --fail --continue-at - --output "$zip_file" "$url"

echo "Unpacking to:"
echo "  $tif_dir"
unzip -o "$zip_file" -d "$tif_dir"

if ! command -v gdal_translate >/dev/null 2>&1; then
	echo
	echo "Unpacked GeoTIFF files only."
	echo "Install GDAL to also create PNG files:"
	echo "  sudo apt install gdal-bin"
	exit 0
fi

echo
echo "Converting monthly GeoTIFFs to Freeminer CDN PNGs..."
for month in $(seq -w 1 12); do
	src="$tif_dir/wc2.1_${resolution}_tavg_${month}.tif"
	dst="$cdn_earth_dir/earth_tavg_${month}.png"

	if [[ ! -s "$src" ]]; then
		echo "Missing $src" >&2
		exit 1
	fi

	# Freeminer currently decodes grayscale 0..255 as -60..60 C.
	gdal_translate -q -of PNG -ot Byte -scale -60 60 0 255 "$src" "$dst"
	echo "  $dst"
done

if command -v gdal_calc.py >/dev/null 2>&1; then
	annual_tif="$work_dir/earth_tavg_annual.tif"
	gdal_calc.py --quiet --overwrite \
		-A "$tif_dir/wc2.1_${resolution}_tavg_01.tif" \
		-B "$tif_dir/wc2.1_${resolution}_tavg_02.tif" \
		-C "$tif_dir/wc2.1_${resolution}_tavg_03.tif" \
		-D "$tif_dir/wc2.1_${resolution}_tavg_04.tif" \
		-E "$tif_dir/wc2.1_${resolution}_tavg_05.tif" \
		-F "$tif_dir/wc2.1_${resolution}_tavg_06.tif" \
		-G "$tif_dir/wc2.1_${resolution}_tavg_07.tif" \
		-H "$tif_dir/wc2.1_${resolution}_tavg_08.tif" \
		-I "$tif_dir/wc2.1_${resolution}_tavg_09.tif" \
		-J "$tif_dir/wc2.1_${resolution}_tavg_10.tif" \
		-K "$tif_dir/wc2.1_${resolution}_tavg_11.tif" \
		-L "$tif_dir/wc2.1_${resolution}_tavg_12.tif" \
		--calc="(A+B+C+D+E+F+G+H+I+J+K+L)/12.0" \
		--outfile="$annual_tif"

	gdal_translate -q -of PNG -ot Byte -scale -60 60 0 255 \
		"$annual_tif" "$cdn_earth_dir/earth_tavg.png"
	echo "  $cdn_earth_dir/earth_tavg.png"
else
	echo
	echo "gdal_calc.py not found; annual earth_tavg.png was not created."
	echo "Monthly files are enough when weather is enabled."
fi

echo
echo "Done."
