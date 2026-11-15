# file: CMakeLists.txt
cmake_minimum_required(VERSION 3.20)
project(logicsim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LOGICSIM_BUILD_TESTS "Build the unit tests" ON)
option(LOGICSIM_SANITIZE "Build Debug with AddressSanitizer + UBSan" ON)

# --- Simulation engine, as a library a GUI (or anything else) can link ------

add_library(logicsim_core STATIC
    src/BuiltinGate.cpp
    src/Clock.cpp
    src/EventQueue.cpp
    src/FlipFlop.cpp
    src/Gate.cpp
    src/Latch.cpp
    src/Module.cpp
    src/Net.cpp
    src/Simulator.cpp
    src/VcdWriter.cpp
    src/Adder.cpp
    src/Mux.cpp
    src/Shifter.cpp
    src/Register.cpp
)

target_include_directories(logicsim_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(MSVC)
    target_compile_options(logicsim_core PUBLIC /W4)
else()
    target_compile_options(logicsim_core PUBLIC -Wall -Wextra -Wpedantic)

    if(LOGICSIM_SANITIZE)
        # The compiler accepting -fsanitize does not mean the runtime
        # libraries (libasan / libubsan) are installed, so test-link first
        # instead of failing at the final link step.
        include(CheckCXXSourceCompiles)
        set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
        set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
        check_cxx_source_compiles("int main() { return 0; }" LOGICSIM_SANITIZERS_LINK)
        unset(CMAKE_REQUIRED_FLAGS)
        unset(CMAKE_REQUIRED_LINK_OPTIONS)

        if(LOGICSIM_SANITIZERS_LINK)
            target_compile_options(logicsim_core PUBLIC
                $<$<CONFIG:Debug>:-fsanitize=address,undefined -fno-omit-frame-pointer>)
            target_link_options(logicsim_core PUBLIC
                $<$<CONFIG:Debug>:-fsanitize=address,undefined>)
        elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
            message(WARNING
                "Sanitizer runtime libraries not found; building Debug without "
                "AddressSanitizer/UBSan. To enable them install the runtimes "
                "(Fedora: sudo dnf install libasan libubsan; Debian/Ubuntu: "
                "they ship with gcc) and re-run cmake with a fresh build dir "
                "(make clean).")
        endif()
    endif()
endif()

# --- Demo program -----------------------------------------------------------

add_executable(logicsim src/main.cpp)
target_link_libraries(logicsim PRIVATE logicsim_core)

# --- Tests ------------------------------------------------------------------

if(LOGICSIM_BUILD_TESTS)
    enable_testing()
    set(LOGICSIM_TEST_FILES
      types
      event_queue
      net
      builtin_gate
      sequential
      simulator
      module
      vcd
      adder
      mux
      shifter
      register
    )
    set(test_sources tests/test_main.cpp)
    foreach(name IN LISTS LOGICSIM_TEST_FILES)
      list(APPEND test_sources tests/test_${name}.cpp)
    endforeach()
    
    add_executable(logicsim_tests ${test_sources})
    target_link_libraries(logicsim_tests PRIVATE logicsim_core)

    foreach(name IN LISTS LOGICSIM_TEST_FILES)
      add_test(NAME ${name} COMMAND logicsim_tests --file test_${name})
    endforeach()
endif()
