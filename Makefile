# Makefile for local_api
#
# Host build:    make
# Android NDK:   make NDK_HOME=/path/to/ndk
# Clean:         make clean

CXX      ?= g++
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

# Project paths
INCLUDES := -Iinclude -Ithird_party
SRC_DIR  := src
BUILD    := build
TARGET_NAME := local_api

# All source files
SRCS     := $(SRC_DIR)/main.cpp \
            $(SRC_DIR)/router.cpp \
            $(SRC_DIR)/tools.cpp \
            $(SRC_DIR)/detector.cpp \
            $(SRC_DIR)/install.cpp

ABIS     := arm64-v8a armeabi-v7a x86_64

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -frtti -fexceptions $(INCLUDES)
HOST_LDFLAGS := -pthread
LDFLAGS  := -llog -pthread

.PHONY: all ndk all-abis clean check-ndk

all:
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) -o $(BUILD)/$(TARGET_NAME) $(SRCS) $(HOST_LDFLAGS)

ndk: check-ndk
	@mkdir -p $(BUILD)
	$(NDK_CXX) $(CXXFLAGS) -fPIE -pie -o $(BUILD)/$(TARGET_NAME)_$(ABI) $(SRCS) $(LDFLAGS)

all-abis:
	@for abi in $(ABIS); do \
		$(MAKE) ABI=$$abi NDK_HOME=$(NDK_HOME) ndk || exit 1; \
	done

check-ndk:
	@if [ -z "$(NDK_HOME)" ]; then \
		echo "Error: NDK_HOME is not set. Use: make NDK_HOME=/path/to/ndk"; exit 1; \
	fi
	@if [ ! -x "$(NDK_CXX)" ]; then \
		echo "Error: compiler not found: $(NDK_CXX)"; exit 1; \
	fi

clean:
	rm -rf $(BUILD)
