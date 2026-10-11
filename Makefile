# Makefile for local_api
#
# Host build:    make
# Android NDK:   make NDK_HOME=/path/to/ndk
# Clean:         make clean
#
# curl is vendored under third_party/curl (HTTP-only, TLS disabled) and
# statically compiled into the binary, so downloads work on any target
# environment without a runtime libcurl.

CXX      ?= g++
CC       ?= gcc
AR       ?= ar
NDK_HOME ?= $(ANDROID_NDK_HOME)
ABI      ?= arm64-v8a
API      ?= 24

UNAME_S  := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
    HOST_TAG := linux-x86_64
else ifeq ($(UNAME_S),Darwin)
    HOST_TAG := darwin-x86_64
else
    HOST_TAG := windows-x86_64
endif

TOOLCHAIN := $(NDK_HOME)/toolchains/llvm/prebuilt/$(HOST_TAG)
ifeq ($(ABI),arm64-v8a)
    TARGET  := aarch64-linux-android
    CXX_ABI := $(TARGET)$(API)-clang++
else ifeq ($(ABI),armeabi-v7a)
    TARGET  := armv7a-linux-androideabi
    CXX_ABI := $(TARGET)$(API)-clang++
else ifeq ($(ABI),x86_64)
    TARGET  := x86_64-linux-android
    CXX_ABI := $(TARGET)$(API)-clang++
else
    $(error Unsupported ABI: $(ABI))
endif

NDK_CXX  := $(TOOLCHAIN)/bin/$(CXX_ABI)
NDK_AR   := $(TOOLCHAIN)/bin/llvm-ar

# Project paths
INCLUDES := -Iinclude -Ithird_party -Ithird_party/curl/include
SRC_DIR  := src
BUILD    := build
TARGET_NAME := local_api

# All source files
SRCS     := $(SRC_DIR)/main.cpp \
            $(SRC_DIR)/router.cpp \
            $(SRC_DIR)/tools.cpp \
            $(SRC_DIR)/detector.cpp \
            $(SRC_DIR)/install.cpp \
            $(SRC_DIR)/debug.cpp \
            $(SRC_DIR)/downloader.cpp

ABIS     := arm64-v8a armeabi-v7a x86_64

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -frtti -fexceptions $(INCLUDES)
HOST_LDFLAGS := -pthread
LDFLAGS  := -llog -pthread

# Vendored curl (HTTP-only): compile once into a static library.
CURL_DIR    := third_party/curl
CURL_SRCS   := $(wildcard $(CURL_DIR)/lib/*.c) \
               $(wildcard $(CURL_DIR)/lib/vauth/*.c) \
               $(wildcard $(CURL_DIR)/lib/vtls/*.c) \
               $(wildcard $(CURL_DIR)/lib/vquic/*.c) \
               $(wildcard $(CURL_DIR)/lib/vssh/*.c) \
               $(wildcard $(CURL_DIR)/lib/curlx/*.c)
CURL_FLAGS  := -DHAVE_CONFIG_H -DBUILDING_LIBCURL -DCURL_STATICLIB \
               -Ithird_party -I$(CURL_DIR)/lib -I$(CURL_DIR)/include

CURL_OBJS_HOST := $(patsubst $(CURL_DIR)/%.c,$(BUILD)/curl/%.o,$(CURL_SRCS))
CURL_OBJS_NDK  := $(patsubst $(CURL_DIR)/%.c,$(BUILD)/curl-ndk/%.o,$(CURL_SRCS))
CURL_LIB_HOST  := $(BUILD)/libcurl_local.a
CURL_LIB_NDK   := $(BUILD)/libcurl_local_ndk.a

.PHONY: all ndk all-abis clean check-ndk

all: $(CURL_LIB_HOST)
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/$(TARGET_NAME) $(SRCS) $(CURL_LIB_HOST) $(HOST_LDFLAGS)

ndk: check-ndk $(CURL_LIB_NDK)
	@mkdir -p $(BUILD)
	$(NDK_CXX) $(CXXFLAGS) -fPIE -pie -o $(BUILD)/$(TARGET_NAME)_$(ABI) $(SRCS) $(CURL_LIB_NDK) $(LDFLAGS)

all-abis:
	@for abi in $(ABIS); do \
		$(MAKE) ABI=$$abi NDK_HOME=$(NDK_HOME) ndk || exit 1; \
	done

$(BUILD)/curl/%.o: $(CURL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CURL_FLAGS) -O2 -w -c $< -o $@

$(CURL_LIB_HOST): $(CURL_OBJS_HOST)
	$(AR) rcs $@ $^

$(BUILD)/curl-ndk/%.o: $(CURL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(TOOLCHAIN)/bin/$(TARGET)$(API)-clang $(CURL_FLAGS) -O2 -w -c $< -o $@

$(CURL_LIB_NDK): $(CURL_OBJS_NDK)
	$(NDK_AR) rcs $@ $^

check-ndk:
	@if [ -z "$(NDK_HOME)" ]; then \
		echo "Error: NDK_HOME is not set. Use: make NDK_HOME=/path/to/ndk"; exit 1; \
	fi
	@if [ ! -x "$(NDK_CXX)" ]; then \
		echo "Error: compiler not found: $(NDK_CXX)"; exit 1; \
	fi

clean:
	rm -rf $(BUILD)
