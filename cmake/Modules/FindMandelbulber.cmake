#
# NOT USED
#

#
# make new patch:
# diff -EbBw --normal -u ../mandelbulber-read-only/src src/mandelbulber | grep -v "Only in" > util/mandelbulber.patch
#

FIND_PATH(MANDELBULBER_PATH NAMES fractal.h PATHS ${PROJECT_SOURCE_DIR}/mandelbulber NO_DEFAULT_PATH)

IF(MANDELBULBER_PATH)
	SET(MANDELBULBER_FOUND 1)
ELSE(MANDELBULBER_PATH)
	IF(ENABLE_MANDELBULBER)
		MESSAGE(STATUS "Checkout mandelbulber to ${PROJECT_SOURCE_DIR}/mandelbulber")
		EXECUTE_PROCESS(COMMAND "svn" "checkout" "-r" "r438" "-q" "http://mandelbulber.googlecode.com/svn/trunk/src" "${PROJECT_SOURCE_DIR}/mandelbulber")
		MESSAGE(STATUS "Patching mandelbulber")
		EXECUTE_PROCESS(COMMAND "patch" "--directory=${PROJECT_SOURCE_DIR}/mandelbulber/" "-i${CMAKE_SOURCE_DIR}/util/mandelbulber.patch")
	ENDIF(ENABLE_MANDELBULBER)
ENDIF(MANDELBULBER_PATH)

FIND_PATH(MANDELBULBER_PATH NAMES fractal.h PATHS ${PROJECT_SOURCE_DIR}/mandelbulber NO_DEFAULT_PATH)
IF(MANDELBULBER_PATH)
	SET(MANDELBULBER_FOUND 1)
ENDIF(MANDELBULBER_PATH)

IF(MANDELBULBER_FOUND)
	SET(USE_MANDELBULBER 1)
	MESSAGE(STATUS "Found mandelbulber source in ${MANDELBULBER_PATH}")
ENDIF(MANDELBULBER_FOUND)
