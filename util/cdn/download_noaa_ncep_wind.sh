#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"

period="${NOAA_NCEP_WIND_PERIOD:-1991-2020}"
force="${NOAA_NCEP_WIND_FORCE:-0}"
out_width="${NOAA_NCEP_WIND_WIDTH:-720}"
out_height="${NOAA_NCEP_WIND_HEIGHT:-361}"
cdn_earth_dir="${1:-$repo_dir/cdn/earth}"

base_url="https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis.derived/surface_gauss"
work_dir="$cdn_earth_dir/noaa_ncep_wind_${period}"
u_nc="$work_dir/uwnd.10m.mon.ltm.${period}.nc"
v_nc="$work_dir/vwnd.10m.mon.ltm.${period}.nc"

usage() {
	echo "Usage: $0 [cdn-earth-dir]"
	echo
	echo "Downloads NOAA PSL NCEP/NCAR 10m monthly wind climatology."
	echo "No token or account is required."
	echo "Default CDN directory: $repo_dir/cdn/earth"
	echo "Period: NOAA_NCEP_WIND_PERIOD=$period"
	echo "Output raster: NOAA_NCEP_WIND_WIDTH=$out_width NOAA_NCEP_WIND_HEIGHT=$out_height"
	echo "Force re-download: NOAA_NCEP_WIND_FORCE=$force"
	echo
	echo "Output expected by Freeminer:"
	echo "  <cdn-earth-dir>/earth_wind_u_01.png .. earth_wind_u_12.png"
	echo "  <cdn-earth-dir>/earth_wind_v_01.png .. earth_wind_v_12.png"
	echo "  <cdn-earth-dir>/earth_wind_u.png and earth_wind_v.png if gdal_calc.py is installed"
	echo
	echo "Requires:"
	echo "  curl"
	echo "  sudo apt install gdal-bin"
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
	echo "curl not found" >&2
	exit 1
fi

for tool in gdal_translate gdalwarp; do
	if ! command -v "$tool" >/dev/null 2>&1; then
		echo "$tool not found." >&2
		echo "Install it with: sudo apt install gdal-bin" >&2
		exit 1
	fi
done

mkdir -p "$cdn_earth_dir" "$work_dir"

download_file() {
	local name="$1"
	local dst="$2"
	local url="$base_url/$name"

	if [[ "$force" != "1" && -s "$dst" ]]; then
		echo "Existing NetCDF found, reusing:"
		echo "  $dst"
		echo "Set NOAA_NCEP_WIND_FORCE=1 to download again."
		return
	fi

	echo "Downloading:"
	echo "  $url"
	echo "to:"
	echo "  $dst"
	curl -L --fail --continue-at - --output "$dst" "$url"
}

warp_band_to_tif() {
	local nc_file="$1"
	local var="$2"
	local band="$3"
	local tmp_tif="$4"
	local warped_tif="$5"

	gdal_translate -q -of GTiff -b "$band" -a_srs EPSG:4326 \
		"NETCDF:${nc_file}:${var}" "$tmp_tif"

	# NOAA stores global longitude as 0..360. Freeminer samples PNGs as
	# -180..180, so write a normalized equirectangular raster.
	gdalwarp -q -overwrite -s_srs EPSG:4326 -t_srs EPSG:4326 \
		-te -180 -90 180 90 -ts "$out_width" "$out_height" -r bilinear \
		"$tmp_tif" "$warped_tif"
}

convert_component() {
	local nc_file="$1"
	local var="$2"
	local out_prefix="$3"

	echo
	echo "Converting $var to $out_prefix..."
	for month in $(seq -w 1 12); do
		local band=$((10#$month))
		local tmp_tif="$work_dir/${out_prefix}_${month}.tif"
		local warped_tif="$work_dir/${out_prefix}_${month}_lon180.tif"
		local dst="$cdn_earth_dir/${out_prefix}_${month}.png"

		warp_band_to_tif "$nc_file" "$var" "$band" "$tmp_tif" "$warped_tif"
		gdal_translate -q -of PNG -ot Byte -scale -20 20 0 255 \
			"$warped_tif" "$dst"
		echo "  $dst"
	done

	if command -v gdal_calc.py >/dev/null 2>&1; then
		local annual_tif="$work_dir/${out_prefix}.tif"
		gdal_calc.py --quiet --overwrite \
			-A "$work_dir/${out_prefix}_01_lon180.tif" \
			-B "$work_dir/${out_prefix}_02_lon180.tif" \
			-C "$work_dir/${out_prefix}_03_lon180.tif" \
			-D "$work_dir/${out_prefix}_04_lon180.tif" \
			-E "$work_dir/${out_prefix}_05_lon180.tif" \
			-F "$work_dir/${out_prefix}_06_lon180.tif" \
			-G "$work_dir/${out_prefix}_07_lon180.tif" \
			-H "$work_dir/${out_prefix}_08_lon180.tif" \
			-I "$work_dir/${out_prefix}_09_lon180.tif" \
			-J "$work_dir/${out_prefix}_10_lon180.tif" \
			-K "$work_dir/${out_prefix}_11_lon180.tif" \
			-L "$work_dir/${out_prefix}_12_lon180.tif" \
			--calc="(A+B+C+D+E+F+G+H+I+J+K+L)/12.0" \
			--outfile="$annual_tif"
		gdal_translate -q -of PNG -ot Byte -scale -20 20 0 255 \
			"$annual_tif" "$cdn_earth_dir/${out_prefix}.png"
		echo "  $cdn_earth_dir/${out_prefix}.png"
	else
		echo
		echo "gdal_calc.py not found; annual ${out_prefix}.png was not created."
		echo "Monthly files are enough when weather is enabled."
	fi
}

echo "Preparing NOAA/NCEP wind climatology:"
echo "  $work_dir"

download_file "uwnd.10m.mon.ltm.${period}.nc" "$u_nc"
download_file "vwnd.10m.mon.ltm.${period}.nc" "$v_nc"

convert_component "$u_nc" "uwnd" "earth_wind_u"
convert_component "$v_nc" "vwnd" "earth_wind_v"

echo
echo "Done."
