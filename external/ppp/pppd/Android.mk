LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	auth.c \
	ccp.c \
	chap-md5.c \
	chap-new.c \
	chap_ms.c \
	demand.c \
	eap.c \
	ecp.c \
	eui64.c \
	fsm.c \
	ipcp.c \
	ipv6cp.c \
	lcp.c \
	magic.c \
	main.c \
	options.c \
	pppcrypt.c \
	pppox.c \
	session.c \
	sys-linux.c \
	tty.c \
	upap.c \
	utils.c

# options.c:623:21: error: passing 'const char *' to parameter of type 'char *' discards qualifiers.
# [-Werror,-Wincompatible-pointer-types-discards-qualifiers]
LOCAL_CLANG_CFLAGS += -Wno-incompatible-pointer-types-discards-qualifiers

LOCAL_SHARED_LIBRARIES := \
	libcutils liblog libcrypto

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include

# Force sizeof(enum)=4
LOCAL_CFLAGS := -fno-short-enums -DCHAPMS=1 -DMPPE=1 -DINET6=1 -DUSE_OPENSSL=1 -Wno-unused-parameter -Wno-empty-body -Wno-missing-field-initializers -Wno-attributes -Wno-sign-compare -Wno-pointer-sign -Werror

# Turn off warnings for now until this is fixed upstream. b/18632512
LOCAL_CFLAGS += -Wno-unused-variable

LOCAL_MODULE:= pppd
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := chat.c
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_MODULE := chat
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
