cmake_minimum_required(VERSION 3.14)

project(hls_server LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core)

add_executable(hls_server
  main.cpp encoder.cpp encoder.h
  segmenter.h
  segmenter.cpp
  server.h
  server.cpp
  connection.h
  connection.cpp
  thread_pool.h
  thread_pool.cpp
  threadsafe_queue.h
)
target_link_libraries(hls_server Qt${QT_VERSION_MAJOR}::Core avutil avcodec avformat swresample avdevice avfilter)
