LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

SRC := $(LOCAL_PATH)/..
OUT := $(LOCAL_PATH)

include $(SRC)/build.mk

LOCAL_MODULE := sigrok_bindings

LOCAL_CFLAGS := $(INCLUDES)
LOCAL_LDLIBS := $(LDFLAGS) $(LIBS)

LOCAL_SRC_FILES := swig_out/libsigrok_wrap.c

include $(BUILD_SHARED_LIBRARY)
