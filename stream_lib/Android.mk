#
#
# Copyright (C) Intel 2013-2016
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

#######################################################################
# Common variables

component_export_include_dir := $(LOCAL_PATH)/include

component_src_files :=  \
    IoStream.cpp \
    TinyAlsaAudioDevice.cpp \
    StreamLib.cpp

component_includes_common := \
    $(component_export_include_dir) \
    $(call include-path-for, frameworks-av) \
    external/tinyalsa/include \
    $(call include-path-for, audio-utils)

component_includes_dir := \
    hw \
    parameter \

component_includes_dir_host := \
    $(foreach inc, $(component_includes_dir), $(HOST_OUT_HEADERS)/$(inc)) \
    $(component_includes_common) \

component_includes_dir_target := \
    $(foreach inc, $(component_includes_dir), $(TARGET_OUT_HEADERS)/$(inc)) \
    $(component_includes_common)
    $(call include-path-for, bionic)

component_static_lib += \
    libsamplespec_static \
    libaudio_comms_utilities \
    audio.routemanager.includes \
    libproperty

component_static_lib_host += \
    $(foreach lib, $(component_static_lib), $(lib)_host) \

component_cflags := -Wall -Werror -Wno-unused-parameter

#######################################################################
# Component Host Build
ifeq (0,1)
include $(CLEAR_VARS)

LOCAL_MODULE := libstream_static_host
LOCAL_MODULE_OWNER := intel

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)
LOCAL_C_INCLUDES := $(component_includes_dir_host)

LOCAL_STATIC_LIBRARIES := $(component_static_lib_host)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags) -O0 -ggdb
LOCAL_MODULE_TAGS := optional
LOCAL_STRIP_MODULE := false

include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_HOST_STATIC_LIBRARY)
endif

#######################################################################
# Component Target Build

include $(CLEAR_VARS)

LOCAL_MODULE := libstream_static
LOCAL_MODULE_OWNER := intel

LOCAL_EXPORT_C_INCLUDE_DIRS := $(component_export_include_dir)

LOCAL_C_INCLUDES := $(component_includes_dir_target)

LOCAL_STATIC_LIBRARIES := $(component_static_lib)
LOCAL_SHARED_LIBRARIES := $(component_dynamic_lib)
LOCAL_SRC_FILES := $(component_src_files)
LOCAL_CFLAGS := $(component_cflags)
LOCAL_MODULE_TAGS := optional


include $(OPTIONAL_QUALITY_COVERAGE_JUMPER)

include $(BUILD_STATIC_LIBRARY)
