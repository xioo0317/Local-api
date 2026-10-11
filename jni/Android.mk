# Android.mk - ndk-build configuration for local_api
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := local_api

# Source files:
#   main.cpp     — entry point, argv dispatch (install / else -> router)
#   router.cpp   — HTTP service (long-running) + request dispatch by action
#   tools.cpp    — tool functions (detect, version, debug, etc.)
#   detector.cpp — KernelSU / APatch / Magisk / SusFS handshake detection
#   install.cpp  — one-shot initialization (run once)
#   debug.cpp    — CLI debug entry (master-switch gated), standalone diagnostics
#   downloader.cpp — download helper (vendored curl, statically linked)
LOCAL_SRC_FILES := \
    ../src/main.cpp \
    ../src/router.cpp \
    ../src/tools.cpp \
    ../src/detector.cpp \
    ../src/install.cpp \
    ../src/debug.cpp \
    ../src/downloader.cpp

# Vendored curl (HTTP-only): compile the library into the executable.
# ndk-build may run from the repo root (NDK_PROJECT_PATH=.), so wildcard
# with LOCAL_PATH-anchored absolute paths, then convert to LOCAL_PATH-
# relative entries (../third_party/...) for LOCAL_SRC_FILES.
CURL_ROOT := $(LOCAL_PATH)/../third_party/curl
CURL_ABS := $(wildcard $(CURL_ROOT)/lib/*.c) \
    $(wildcard $(CURL_ROOT)/lib/vauth/*.c) \
    $(wildcard $(CURL_ROOT)/lib/vtls/*.c) \
    $(wildcard $(CURL_ROOT)/lib/vquic/*.c) \
    $(wildcard $(CURL_ROOT)/lib/vssh/*.c) \
    $(wildcard $(CURL_ROOT)/lib/curlx/*.c)
CURL_FILES := $(addprefix ../,$(subst $(LOCAL_PATH)/../,,$(CURL_ABS)))
LOCAL_SRC_FILES += $(CURL_FILES)

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../third_party \
    $(LOCAL_PATH)/../third_party/curl/lib \
    $(LOCAL_PATH)/../third_party/curl/include

LOCAL_CPPFLAGS := -std=c++17 -frtti -fexceptions -Wall -Wextra
LOCAL_CFLAGS := -DHAVE_CONFIG_H -DBUILDING_LIBCURL -DCURL_STATICLIB -w

LOCAL_LDLIBS := -llog -pthread

include $(BUILD_EXECUTABLE)
