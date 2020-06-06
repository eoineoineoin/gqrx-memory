cmake_minimum_required(VERSION 2.4)

project(gqrx-memory)

set (CMAKE_CXX_STANDARD 17)
find_package(X11 REQUIRED)
find_package(PulseAudio)
include_directories(. ${X11_INCLUDE_DIR} ${PULSEAUDIO_INCLUDE_DIR})
add_executable(gqrx-memory main.cpp XlibKeyConnection.cpp GqrxConnection.cpp AudioPlayer.cpp)

if(PULSEAUDIO_FOUND)
	# This lib is not included by default:
	target_link_libraries(gqrx-memory pulse-simple)
	add_compile_definitions(HAS_PULSEAUDIO)
endif()

target_link_libraries(gqrx-memory ${X11_LIBRARIES} ${PULSEAUDIO_LIBRARY})
