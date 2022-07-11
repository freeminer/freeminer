
# https://github.com/ClickHouse/ClickHouse.git base/base/

set(DEBUG_SRCS ${DEBUG_SRCS}
	${CMAKE_CURRENT_SOURCE_DIR}/getThreadId.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/demangle.cpp
	PARENT_SCOPE)
