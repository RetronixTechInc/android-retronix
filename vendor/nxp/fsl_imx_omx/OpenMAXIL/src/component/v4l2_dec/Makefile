#####################################################################################
#
#   Copyright (c) 2010-2011, Freescale Semiconductors Inc.,
#   All Rights Reserved.
# 
#   The following programs are the sole property of Freescale Semiconductors Inc.,
#   and contain its proprietary and confidential information.
# 
####################################################################################### 
#
#   This file will compile all cpp file and build dynimic library. 
#
######################################################################################


#Set the root directory
OMX_ROOT_DIR=../../../..

#Set the component directory
OMX_COMPONENT_DEFINE_DIR=..

V4L2_COMMON_DEFINE_DIR=../v4l2_common

# Set the BUILD information 
include $(OMX_COMPONENT_DEFINE_DIR)/Makefile_component.defines

COMPONENT_NAME=v4l2_dec
COMPONENT_SO_NAME=lib_omx_$(COMPONENT_NAME)_$(PROCESSOR)_$(OSTYPE).so

LFLAGS+= -l_omx_v4l2_common_$(PROCESSOR)_$(OSTYPE)

CXXFLAGS+= -I$(V4L2_COMMON_DEFINE_DIR)

SOURCE_FILES = $(SRC_DIR)/V4l2Dec.cpp \


#	$(SRC_DIR)/../v4l2_common/FrameConverter.cpp \
#	$(SRC_DIR)/../v4l2_common/FrameParser.cpp \
#	$(SRC_DIR)/../v4l2_common/Process2.cpp \
#	$(SRC_DIR)/../v4l2_common/V4l2Device.cpp \
#	$(SRC_DIR)/../v4l2_common/V4l2Filter.cpp \
#	$(SRC_DIR)/../v4l2_common/V4l2Object.cpp \
#	$(SRC_DIR)/../v4l2_common/VThread.cpp \


LFLAGS +=-L$(BSP_RELATED_LIB_DIR)
#Include rules Makefile
include $(OMX_COMPONENT_DEFINE_DIR)/Makefile.rules	



