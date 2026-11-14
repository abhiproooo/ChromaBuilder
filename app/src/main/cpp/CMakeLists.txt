cmake_minimum_required(VERSION 3.22.1)

project("chromatoons_native" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(native-lib SHARED
    native-lib.cpp
    json.cpp
    dragonbones.cpp
    renderer.cpp
)

find_library(log-lib log)
target_link_libraries(native-lib ${log-lib} android)
