# Copyright 2019 NXP
#

CFLAGS += -DLINUX -fPIC

ifeq ($(ANDROID_BUILD), 1)
CFLAGS += -pie -fPIE
endif

INCLUDES = -I../include

LIB_DIR = ../release/lib
RELEASE_DIR = ../release

TEST_OBJS = ./src/test.o \
		./src/wave_func.o

ifeq ($(ANDROID_BUILD), 1)
LIBRARY = $(RELEASE_DIR)/lib/lib_dspc_asrc_android.so
TARGET=asrc-test_android
else
LIBRARY = $(RELEASE_DIR)/lib/lib_dspc_asrc.so
TARGET=asrc-test
endif

all: ASRC_TEST
	echo "--- Build DSPC ASRC TEST ---"

ASRC_TEST: $(TEST_OBJS)
ifneq ($(ARCH), arm64)
	@echo "--only support ARM64 build---"
	@exit 1;
endif
	echo "--- Build unit test ---"
	cp $(LIBRARY) ./
	$(CC) $(CFLAGS) $(TEST_OBJS) $(subst $(LIB_DIR)/,,$(LIBRARY)) -lm -o $(TARGET)
	cp $(TARGET) $(RELEASE_DIR)/exe/

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f ./src/*.o
	rm -f asrc-test

help:
	@echo "targets are:"
	@echo "\tASRC_TEST\t- build asrc unit test"
	@echo "\tall\t\t- build the above"
