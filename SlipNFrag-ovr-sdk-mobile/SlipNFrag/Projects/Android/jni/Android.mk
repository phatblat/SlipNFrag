
LOCAL_PATH:= $(call my-dir)

#--------------------------------------------------------
# libslipnfrag.so
#--------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE			:= slipnfrag
ID1_SRC_FILES           := $(wildcard $(LOCAL_PATH)/../../../../../id1/*.cpp)
ID1_SRC_FILES           := $(filter-out $(LOCAL_PATH)/../../../../../id1/snd_dma.cpp, $(ID1_SRC_FILES))
ID1_SRC_FILES           := $(filter-out $(LOCAL_PATH)/../../../../../id1/snd_mem.cpp, $(ID1_SRC_FILES))
ID1_SRC_FILES           := $(filter-out $(LOCAL_PATH)/../../../../../id1/snd_mix.cpp, $(ID1_SRC_FILES))
ID1_SRC_FILES           := $(filter-out $(LOCAL_PATH)/../../../../../id1/net_win.cpp, $(ID1_SRC_FILES))
ID1_SRC_FILES           := $(filter-out $(LOCAL_PATH)/../../../../../id1/net_wins.cpp, $(ID1_SRC_FILES))
NULL_SRC_FILES          := $(wildcard $(LOCAL_PATH)/../../../../../SlipNFrag/*.cpp)
NULL_SRC_FILES          := $(filter-out $(LOCAL_PATH)/../../../../../SlipNFrag/in_null.cpp, $(NULL_SRC_FILES))
NULL_SRC_FILES          := $(filter-out $(LOCAL_PATH)/../../../../../SlipNFrag/sys_null.cpp, $(NULL_SRC_FILES))
NULL_SRC_FILES          := $(filter-out $(LOCAL_PATH)/../../../../../SlipNFrag/vid_null.cpp, $(NULL_SRC_FILES))
LOCAL_SRC_FILES			:=  ../../../Src/in_ovr.cpp ../../../Src/main.cpp ../../../Src/sys_ovr.cpp ../../../Src/vid_ovr.cpp
LOCAL_SRC_FILES         += $(ID1_SRC_FILES)
LOCAL_SRC_FILES         += $(NULL_SRC_FILES)
LOCAL_C_INCLUDES        += $(LOCAL_PATH)/../../../../../id1
LOCAL_LDLIBS			:= -llog -landroid -lGLESv3 -lEGL		# include default libraries

LOCAL_LDFLAGS			:= -u ANativeActivity_onCreate

LOCAL_STATIC_LIBRARIES	:= android_native_app_glue 
LOCAL_SHARED_LIBRARIES	:= vrapi

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)
$(call import-module,VrApi/Projects/AndroidPrebuilt/jni)
