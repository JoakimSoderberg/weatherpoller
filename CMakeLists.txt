cmake_minimum_required (VERSION 2.8)

project(weatherpoller C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(WSP_SRCS
	wsp.c
	memory.c
	wspusb.c
	utils.c
	output.c
	weather.c)

set(WSP_HDRS
	wsp.h
	wspusb.h
	weather.h
	utils.h
	memory.h)

if (WIN32)
	list(APPEND WSP_SRCS win32/getopt.c)
	list(APPEND WSP_HDRS win32/getopt.h)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/win32)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

source_group("Headers"		FILES ${WSP_HDRS})
source_group("Source files" 	FILES ${WSP_SRCS})

add_executable(wsp ${WSP_SRCS} ${WSP_HDRS})

find_package(LibUSB REQUIRED)

message("Found libusb: ${LibUSB_LIBRARIES}")

include_directories(${LibUSB_INCLUDE_DIRS})
target_link_libraries(wsp ${LibUSB_LIBRARIES})

if (UNIX)
	target_link_libraries(wsp m)
endif()

install(TARGETS wsp DESTINATION bin)

