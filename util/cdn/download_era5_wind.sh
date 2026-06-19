#!/bin/bash

set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd "$script_dir/.." && pwd)"

year="${ERA5_WIND_YEAR:-2024}"
grid="${ERA5_WIND_GRID:-0.25}"
force="${ERA5_WIND_FORCE:-0}"
cds_url="${ERA5_CDS_URL:-${CDSAPI_URL:-https://cds.climate.copernicus.eu/api}}"
cds_key="${ERA5_CDS_KEY:-${CDSAPI_KEY:-}}"
cds_rc="${CDSAPI_RC:-${HOME:-}/.cdsapirc}"
cdn_earth_dir="${1:-$repo_dir/cdn/earth}"

work_dir="$cdn_earth_dir/era5_wind_${year}"
nc_file="$work_dir/era5_wind_${year}.nc"

usage() {
	echo "Usage: $0 [cdn-earth-dir]"
	echo
	echo "Downloads ERA5 monthly 10m wind components and prepares CDN PNGs."
	echo "Default CDN directory: $repo_dir/cdn/earth"
	echo "Year: ERA5_WIND_YEAR=$year"
	echo "Grid: ERA5_WIND_GRID=$grid degrees"
	echo "Force re-download: ERA5_WIND_FORCE=$force"
	echo
	echo "Output expected by Freeminer:"
	echo "  <cdn-earth-dir>/earth_wind_u_01.png .. earth_wind_u_12.png"
	echo "  <cdn-earth-dir>/earth_wind_v_01.png .. earth_wind_v_12.png"
	echo "  <cdn-earth-dir>/earth_wind_u.png and earth_wind_v.png if gdal_calc.py is installed"
	echo
	echo "Requires:"
	echo "  python3 -m pip install cdsapi"
	echo "  a configured ~/.cdsapirc or ERA5_CDS_KEY/CDSAPI_KEY"
	echo "  sudo apt install gdal-bin"
}

print_cds_credentials_help() {
	echo "ERA5 download needs Copernicus CDS credentials." >&2
	echo >&2
	echo "Create ~/.cdsapirc:" >&2
	echo "  url: $cds_url" >&2
	echo "  key: YOUR_CDS_TOKEN" >&2
	echo >&2
	echo "Or pass the token only for this run:" >&2
	echo "  ERA5_CDS_KEY=YOUR_CDS_TOKEN $0 [cdn-earth-dir]" >&2
	echo >&2
	echo "No-token lower-resolution alternative:" >&2
	echo "  $script_dir/download_noaa_ncep_wind.sh [cdn-earth-dir]" >&2
	echo >&2
	echo "Token page: https://cds.climate.copernicus.eu/profile" >&2
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
	usage
	exit 0
fi

if ! command -v python3 >/dev/null 2>&1; then
	echo "python3 not found" >&2
	exit 1
fi

if ! python3 -c 'import cdsapi' >/dev/null 2>&1; then
	echo "Python module cdsapi not found." >&2
	echo "Install it with: python3 -m pip install cdsapi" >&2
	exit 1
fi

if ! command -v gdal_translate >/dev/null 2>&1; then
	echo "gdal_translate not found." >&2
	echo "Install it with: sudo apt install gdal-bin" >&2
	exit 1
fi

mkdir -p "$cdn_earth_dir" "$work_dir"

echo "Downloading ERA5 monthly 10m wind for $year:"
echo "  $nc_file"
echo "Script:"
echo "  $(realpath "$0")"
if [[ -n "$cds_key" ]]; then
	echo "CDS credentials: environment"
else
	echo "CDS credentials: $cds_rc"
fi

if [[ "$force" != "1" && -s "$nc_file" ]]; then
	echo "Existing NetCDF found, reusing it."
	echo "Set ERA5_WIND_FORCE=1 to download again."
else
	if [[ -z "$cds_key" && ! -s "$cds_rc" ]]; then
		print_cds_credentials_help
		exit 1
	fi

	ERA5_WIND_YEAR="$year" ERA5_WIND_GRID="$grid" ERA5_WIND_OUT="$nc_file" \
		ERA5_CDS_URL="$cds_url" ERA5_CDS_KEY="$cds_key" python3 - <<'PY'
import os
import sys
import cdsapi

year = os.environ["ERA5_WIND_YEAR"]
grid = os.environ["ERA5_WIND_GRID"]
target = os.environ["ERA5_WIND_OUT"]
cds_url = os.environ.get("ERA5_CDS_URL")
cds_key = os.environ.get("ERA5_CDS_KEY")

try:
    if cds_key:
        client = cdsapi.Client(url=cds_url, key=cds_key)
    else:
        client = cdsapi.Client()
except Exception as exc:
    print(f"Could not configure CDS API client: {exc}", file=sys.stderr)
    print("Set ERA5_CDS_KEY or create a valid ~/.cdsapirc.", file=sys.stderr)
    raise SystemExit(1)

client.retrieve(
    "reanalysis-era5-single-levels-monthly-means",
    {
        "product_type": ["monthly_averaged_reanalysis"],
        "variable": [
            "10m_u_component_of_wind",
            "10m_v_component_of_wind",
        ],
        "year": [year],
        "month": [f"{month:02d}" for month in range(1, 13)],
        "time": ["00:00"],
        "area": [90, -180, -90, 180],
        "grid": [grid, grid],
        "data_format": "netcdf",
        "download_format": "unarchived",
    },
    target,
)
PY
fi

convert_component() {
	local var="$1"
	local out_prefix="$2"

	echo
	echo "Converting $var to $out_prefix..."
	for month in $(seq -w 1 12); do
		local band=$((10#$month))
		local dst="$cdn_earth_dir/${out_prefix}_${month}.png"
		gdal_translate -q -of PNG -ot Byte -b "$band" -scale -20 20 0 255 \
			"NETCDF:${nc_file}:${var}" "$dst"
		echo "  $dst"
	done

	if command -v gdal_calc.py >/dev/null 2>&1; then
		local annual_tif="$work_dir/${out_prefix}.tif"
		gdal_calc.py --quiet --overwrite \
			-A "NETCDF:${nc_file}:${var}" --A_band=1 \
			-B "NETCDF:${nc_file}:${var}" --B_band=2 \
			-C "NETCDF:${nc_file}:${var}" --C_band=3 \
			-D "NETCDF:${nc_file}:${var}" --D_band=4 \
			-E "NETCDF:${nc_file}:${var}" --E_band=5 \
			-F "NETCDF:${nc_file}:${var}" --F_band=6 \
			-G "NETCDF:${nc_file}:${var}" --G_band=7 \
			-H "NETCDF:${nc_file}:${var}" --H_band=8 \
			-I "NETCDF:${nc_file}:${var}" --I_band=9 \
			-J "NETCDF:${nc_file}:${var}" --J_band=10 \
			-K "NETCDF:${nc_file}:${var}" --K_band=11 \
			-L "NETCDF:${nc_file}:${var}" --L_band=12 \
			--calc="(A+B+C+D+E+F+G+H+I+J+K+L)/12.0" \
			--outfile="$annual_tif"
		gdal_translate -q -of PNG -ot Byte -scale -20 20 0 255 \
			"$annual_tif" "$cdn_earth_dir/${out_prefix}.png"
		echo "  $cdn_earth_dir/${out_prefix}.png"
	else
		echo "gdal_calc.py not found; annual ${out_prefix}.png was not created."
	fi
}

convert_component "u10" "earth_wind_u"
convert_component "v10" "earth_wind_v"

echo
echo "Done."
