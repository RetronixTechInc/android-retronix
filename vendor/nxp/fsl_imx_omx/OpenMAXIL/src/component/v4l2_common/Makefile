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

# Set the BUILD information 
include $(OMX_COMPONENT_DEFINE_DIR)/Makefile_component.defines

COMPONENT_NAME=v4l2_common
COMPONENT_SO_NAME=lib_omx_$(COMPONENT_NAME)_$(PROCESSOR)_$(OSTYPE).so

#LFLAGS+= -l_WMV789_dec_$(PROCESSOR)_$(OSTYPE)

SOURCE_FILES = $(SRC_DIR)/V4l2Filter.cpp \
	$(SRC_DIR)/V4l2Object.cpp \
	$(SRC_DIR)/VThread.cpp \
	$(SRC_DIR)/V4l2Device.cpp \
	$(SRC_DIR)/FrameParser.cpp \
	$(SRC_DIR)/FrameConverter.cpp \
	$(SRC_DIR)/Process2.cpp

	
LFLAGS +=-L$(BSP_RELATED_LIB_DIR)

#Include rules Makefile
include $(OMX_COMPONENT_DEFINE_DIR)/Makefile.rules	



