cmake_minimum_required(VERSION 3.28)
project(SwayPlugins CXX)

if (GENERATOR_IS_MULTI_CONFIG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)
else()
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED 23)
set(CMAKE_CXX_STANDARD 23)

add_compile_options(-stdlib=libc++ -fno-exceptions)
add_link_options(-stdlib=libc++)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR})

find_package(simdjson CONFIG REQUIRED)


file(REAL_PATH "${CMAKE_SOURCE_DIR}/../waybar/.local/bin" RUNTIME_INSTALL_DIR)
file(REAL_PATH "${CMAKE_SOURCE_DIR}/../waybar/.local/lib" LIBRARY_INSTALL_DIR)

message(${LIBRARY_INSTALL_DIR})

set(CMAKE_INSTALL_RPATH ${LIBRARY_INSTALL_DIR})

file(GLOB_RECURSE SWAY_IPC_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/sway_ipc/*)
add_library(sway_ipc ${SWAY_IPC_SOURCES})
target_link_libraries(sway_ipc PUBLIC simdjson::simdjson)

target_include_directories(sway_ipc PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(mode_watcher
    mode_watcher.cpp print_error.hpp)
target_link_libraries(mode_watcher PRIVATE sway_ipc)

add_executable(scratchpad_watcher
    scratchpad_watcher.cpp print_error.hpp)
target_link_libraries(scratchpad_watcher PRIVATE sway_ipc)

install(TARGETS sway_ipc mode_watcher scratchpad_watcher
    RUNTIME DESTINATION ${RUNTIME_INSTALL_DIR}
    LIBRARY DESTINATION ${LIBRARY_INSTALL_DIR})

# terrible
set_target_properties(CONAN_LIB::simdjson_simdjson_RELEASE PROPERTIES VERSION 23.0.0 SOVERSION 23)
# set_target_properties(CONAN_LIB::simdjson_simdjson_DEBUG PROPERTIES VERSION 23.0.0 SOVERSION 23)

get_target_property(SIMDJSON_LIB_FILE CONAN_LIB::simdjson_simdjson_RELEASE IMPORTED_LOCATION_RELEASE)
# get_target_property(SIMDJSON_LIB_FILE CONAN_LIB::simdjson_simdjson_DEBUG IMPORTED_LOCATION_RELEASE)
cmake_path(GET SIMDJSON_LIB_FILE PARENT_PATH SIMDJSON_LIB_DIR)

install(DIRECTORY ${SIMDJSON_LIB_DIR}
    # CONFIGURATIONS Release
    CONFIGURATIONS Debug
    DESTINATION ${CMAKE_SOURCE_DIR}/../waybar/.local)

# install(DIRECTORY ${CMAKE_SOURCE_DIR}/bin
#     DESTINATION ${CMAKE_SOURCE_DIR}/../waybar/.local
#     FILE_PERMISSIONS
#         OWNER_READ;OWNER_WRITE;OWNER_EXECUTE)

if (IS_SYMLINK ${SIMDJSON_LIB_FILE})
    file(REAL_PATH ${SIMDJSON_LIB_FILE} REAL_SIMDJSON_LIB_FILE)
    cmake_path(GET SIMDJSON_LIB_FILE FILENAME SIMDJSON_LIB_FILENAME)
    set(SIMDJSON_LIB_FILE ${REAL_SIMDJSON_LIB_FILE})
endif()

