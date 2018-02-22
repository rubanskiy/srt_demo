cmake_minimum_required(VERSION 3.4.1)
set(CMAKE_VERBOSE_MAKEFILE on)

add_library(libcrypto STATIC IMPORTED)
add_library(libssl STATIC IMPORTED)
add_library(libsrt STATIC IMPORTED)

set_target_properties(libcrypto
                      PROPERTIES
                      IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/static/${ANDROID_ABI}/libcrypto.a)

set_target_properties(libssl
                      PROPERTIES
                      IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/static/${ANDROID_ABI}/libcrypto.a)

set_target_properties(libsrt
                      PROPERTIES
                      IMPORTED_LOCATION
                      ${CMAKE_SOURCE_DIR}/static/${ANDROID_ABI}/libsrt.a)

include_directories(/home/alex/work/alex/Android/Ndk/android-ndk-r16b/sysroot/usr/include)

include_directories(src/main/cpp/include)
include_directories(src/main/cpp/include/openssl)
include_directories(src/main/cpp/include/srt/common)
include_directories(src/main/cpp/include/srt/haicrypt)
include_directories(src/main/cpp/include/srt/srtcore)

add_library(transmit-lib SHARED src/main/cpp/native-lib.cpp)

find_library(log-lib log)

target_link_libraries(transmit-lib
                      libcrypto
                      libssl
                      libsrt
                      ${log-lib})