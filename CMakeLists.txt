# file: CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(logicsim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES
    src/main.cpp
    src/Gate.cpp
    src/Net.cpp
    src/Module.cpp
    src/EventQueue.cpp
    src/Simulator.cpp
    src/BuiltinGate.cpp
)

add_executable(logicsim ${SOURCES})

target_include_directories(logicsim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(logicsim PRIVATE -Wall -Wextra -Wpedantic)
