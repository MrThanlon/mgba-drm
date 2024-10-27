cmake_minimum_required(VERSION 3.6)

project(test-mgba)

set(BUILD_QT OFF)
set(BUILD_SDL OFF)
set(BUILD_GL OFF)
set(BUILD_GLES2 OFF)
set(BUILD_GLES3 OFF)
set(USE_EPOXY OFF)
add_subdirectory(../mgba ${CMAKE_BINARY_DIR}/mgba)

find_package(OpenCV REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)
target_include_directories(${PROJECT_NAME} PUBLIC ${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} mgba ${OpenCV_LIBS})
