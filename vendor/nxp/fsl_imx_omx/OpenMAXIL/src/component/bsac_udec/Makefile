#####################################################################################
#
#  Copyright 2017 NXP
#
#  The following programs are the sole property of NXP,
#  and contain its proprietary and confidential information.
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

COMPONENT_NAME=bsac_dec
COMPONENT_SO_NAME=lib_omx_$(COMPONENT_NAME)_v2_$(PROCESSOR)_$(OSTYPE).so

LFLAGS+= -l_$(COMPONENT_NAME)_$(PROCESSOR)_$(OSTYPE)

SOURCE_FILES = $(SRC_DIR)/BsacDec.cpp \

#Include rules Makefile
include $(OMX_COMPONENT_DEFINE_DIR)/Makefile.rules



