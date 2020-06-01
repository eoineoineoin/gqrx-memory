cmake_minimum_required(VERSION 2.4)

project(gqrx-memory)

set (CMAKE_CXX_STANDARD 17)
find_package(X11 REQUIRED)
include_directories(. ${X11_INCLUDE_DIR})
add_executable(gqrx-memory main.cpp XlibKeyConnection.cpp GqrxConnection.cpp)
target_link_libraries(gqrx-memory ${X11_LIBRARIES})
