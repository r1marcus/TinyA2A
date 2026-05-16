# CMakeLists.txt — TinyA2A reference C library
#
# Native build (Linux / macOS / Windows MinGW):
#   mkdir build && cd build && cmake .. && cmake --build .
#
# ESP-IDF component:
#   Drop the entire tinya2a/ folder under your project's components/
#   directory. ESP-IDF will pick up the component via this file.
#
# Arduino: see library.properties / examples in extras/.

cmake_minimum_required(VERSION 3.13)

# ESP-IDF component path. ESP-IDF defines IDF_VER when configuring.
if(DEFINED IDF_TARGET)
    idf_component_register(
        SRCS        "src/envelope.c" "src/journal.c" "src/queue.c" "src/frame.c"
        INCLUDE_DIRS "include"
    )
    return()
endif()

# Native build path
project(tinya2a VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

add_compile_options(-Wall -Wextra -Wpedantic)

set(TINYA2A_SOURCES
    src/envelope.c
    src/journal.c
    src/queue.c
    src/frame.c
)

add_library(tinya2a SHARED ${TINYA2A_SOURCES})
target_include_directories(tinya2a PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(tinya2a PROPERTIES
    VERSION    ${PROJECT_VERSION}
    SOVERSION  0
    OUTPUT_NAME tinya2a
)

add_library(tinya2a_static STATIC ${TINYA2A_SOURCES})
target_include_directories(tinya2a_static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(tinya2a_static PROPERTIES OUTPUT_NAME tinya2a)

# Install rules
include(GNUInstallDirs)
install(TARGETS tinya2a tinya2a_static
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Tests (optional, enabled with -DTINYA2A_BUILD_TESTS=ON if you add tests/)
option(TINYA2A_BUILD_TESTS "Build C tests" OFF)
if(TINYA2A_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_c.c")
    enable_testing()
    add_executable(tinya2a_test_c tests/test_c.c)
    target_link_libraries(tinya2a_test_c tinya2a_static)
    add_test(NAME tinya2a_c_tests COMMAND tinya2a_test_c)
endif()
