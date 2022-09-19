
# https://github.com/ClickHouse/ClickHouse.git base/base/

set(DEBUG_SRCS ${DEBUG_SRCS}
	${CMAKE_CURRENT_SOURCE_DIR}/getThreadId.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/demangle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/stacktrace.cpp
	PARENT_SCOPE)

set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS} "-Wl,-export-dynamic")
