cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_VERBOSE_MAKEFILE on)

add_library(libcrypto STATIC IMPORTED)
add_library(libsrt STATIC IMPORTED)

set_target_properties(libcrypto
                      PROPERTIES
                      IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/static/${ANDROID_ABI}/libcrypto.a)

set_target_properties(libsrt
                      PROPERTIES
                      IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/static/${ANDROID_ABI}/libsrt.a)

include_directories(src/main/cpp/include)
include_directories(src/main/cpp/include/srt/common)
include_directories(src/main/cpp/include/srt/haicrypt)
include_directories(src/main/cpp/include/srt/srtcore)

add_library(transmit-lib SHARED src/main/cpp/transmit-lib.cpp)

find_library(log-lib log)

target_link_libraries(transmit-lib
                      libsrt
                      libcrypto
                      ${log-lib})