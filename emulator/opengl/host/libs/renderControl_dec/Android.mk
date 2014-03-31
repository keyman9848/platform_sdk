LOCAL_PATH := $(call my-dir)

host_common_debug_CFLAGS :=

#For gl debbuging
ifdef GL_DEBUG
host_common_debug_CFLAGS += -ggdb3 -O0 -DCHECK_GL_ERROR
endif

### host library ############################################
$(call emugl-begin-host-static-library,lib_renderControl_dec)
$(call emugl-import,libOpenglCodecCommon)
$(call emugl-gen-decoder,$(EMUGL_PATH)/system/renderControl_enc,renderControl)
# For renderControl_types.h
$(call emugl-export,C_INCLUDES,$(EMUGL_PATH)/system/renderControl_enc)
$(call emugl-export,CFLAGS,$(host_common_debug_CFLAGS))
$(call emugl-end-module)

### host library, 64-bit ####################################
$(call emugl-begin-host-static-library,lib64_renderControl_dec)
$(call emugl-import,lib64OpenglCodecCommon)
$(call emugl-gen-decoder,$(EMUGL_PATH)/system/renderControl_enc,renderControl)
# For renderControl_types.h
$(call emugl-export,C_INCLUDES,$(EMUGL_PATH)/system/renderControl_enc)
$(call emugl-export,CFLAGS,$(host_common_debug_CFLAGS) -m64)
ifeq ($(HOST_OS),windows)
LOCAL_CC = /usr/bin/amd64-mingw32msvc-gcc 
LOCAL_CXX = /usr/bin/amd64-mingw32msvc-g++
endif
$(call emugl-end-module)
