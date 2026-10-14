cmake_minimum_required(VERSION 3.16)
project(local_api CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(APP_SOURCES
  src/main.cpp
  src/router.cpp
  src/tools.cpp
  src/detector.cpp
  src/install.cpp
  src/debug.cpp
  src/downloader.cpp
)

if(ANDROID)
  # Android (NDK toolchain):
  #   cmake -B build-android \
  #     -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake \
  #     -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24
  set(PREBUILT_DIR ${CMAKE_SOURCE_DIR}/third_party/prebuilt/android-arm64)
  set(SYSTEM_LIBS log)
else()
  # Host (linux x86_64)
  set(PREBUILT_DIR ${CMAKE_SOURCE_DIR}/third_party/prebuilt/linux-x86_64)
  find_package(Threads REQUIRED)
  set(SYSTEM_LIBS Threads::Threads)
endif()

add_executable(local_api ${APP_SOURCES})

target_include_directories(local_api PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/third_party
  ${CMAKE_SOURCE_DIR}/third_party/prebuilt/include
)

target_compile_definitions(local_api PRIVATE CURL_STATICLIB)

# Static archive link order matters: curl -> mbedtls -> x509 -> crypto
target_link_libraries(local_api PRIVATE
  ${PREBUILT_DIR}/libcurl.a
  ${PREBUILT_DIR}/libmbedtls.a
  ${PREBUILT_DIR}/libmbedx509.a
  ${PREBUILT_DIR}/libmbedcrypto.a
  ${SYSTEM_LIBS}
)

target_compile_options(local_api PRIVATE -Wall -Wextra -O2)
