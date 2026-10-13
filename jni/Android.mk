# Android.mk - ndk-build configuration for local_api
LOCAL_PATH := $(call my-dir)

# Prebuilt libcurl (HTTP-only, no TLS) built by the xioo0317/curl fork CI.
# The archive is built with -fPIC for android arm64-v8a (API 24).
include $(CLEAR_VARS)
LOCAL_MODULE := curl-prebuilt
LOCAL_SRC_FILES := ../third_party/prebuilt/android-arm64/libcurl.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_MODULE := local_api

# Source files:
#   main.cpp     — entry point, argv dispatch (install / else -> router)
#   router.cpp   — HTTP service (long-running) + request dispatch by action
#   tools.cpp    — tool functions (detect, version, debug, etc.)
#   detector.cpp — KernelSU / APatch / Magisk / SusFS handshake detection
#   install.cpp  — one-shot initialization (run once)
#   debug.cpp    — CLI debug entry (master-switch gated), standalone diagnostics
#   downloader.cpp — download helper (prebuilt libcurl, statically linked)
LOCAL_SRC_FILES := \
    ../src/main.cpp \
    ../src/router.cpp \
    ../src/tools.cpp \
    ../src/detector.cpp \
    ../src/install.cpp \
    ../src/debug.cpp \
    ../src/downloader.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/prebuilt/include

LOCAL_CPPFLAGS := -std=c++17 -frtti -fexceptions -Wall -Wextra -DCURL_STATICLIB

LOCAL_STATIC_LIBRARIES := curl-prebuilt
LOCAL_LDLIBS := -llog -pthread

include $(BUILD_EXECUTABLE)
