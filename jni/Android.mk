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
LOCAL_SRC_FILES := \
    ../src/main.cpp \
    ../src/router.cpp \
    ../src/tools.cpp \
    ../src/detector.cpp \
    ../src/install.cpp

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../third_party

LOCAL_CPPFLAGS := -std=c++17 -frtti -fexceptions -Wall -Wextra

LOCAL_LDLIBS := -llog -pthread

include $(BUILD_EXECUTABLE)
