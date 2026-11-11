Earth climate raster inputs
===========================

WorldClim temperature and vapor pressure cover land. The download scripts
produce monthly images and versioned annual images with alpha marking missing
data. Put these PNG files in the engine's cache/earth directory. Legacy annual
images cannot distinguish opaque missing pixels from valid black pixels; the
engine treats black in those legacy annual files as missing. Versioned images
preserve valid black values.

Optional global temperature images may be placed in the same directory as
`earth_global_tavg.png` and `earth_global_tavg_01.png` through
`earth_global_tavg_12.png`. They must be equirectangular, cover longitude
-180..180 and latitude 90..-90, and encode temperature as grayscale from
-60 C (0) to 60 C (255), with alpha zero for missing samples. Use monthly
2-metre air-temperature climatologies, not sea-surface temperature, and
respect the source's license. WorldClim is preferred where it is valid;
these images fill gaps. They are not downloaded by the engine.

When no valid temperature is available, the engine estimates it from latitude
and season; missing humidity uses an approximate relative humidity. These are
fallbacks, not measurements. Existing loaded weather converges over time.
