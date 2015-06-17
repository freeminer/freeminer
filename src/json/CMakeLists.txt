if(MSVC)
	set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /D NDEBUG")
endif()

include_directories(${PROJECT_SOURCE_DIR})
add_library(jsoncpp jsoncpp.cpp)
target_link_libraries(jsoncpp)

