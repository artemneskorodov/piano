cmake_minimum_required(VERSION 3.29)
project(MidiParser LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR}/lib)

file(GLOB_RECURSE LIB_SOURCES
    ${CMAKE_SOURCE_DIR}/lib/*.cc
)

add_library(midi_lib STATIC ${LIB_SOURCES})

file(GLOB_RECURSE TEST_SOURCES
    test/*.cc
)

add_executable(midi_test ${TEST_SOURCES})

target_link_libraries(midi_test PRIVATE midi_lib)

target_include_directories(midi_test PRIVATE include)
