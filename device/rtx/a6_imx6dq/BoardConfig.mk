#
# Product-specific compile-time definitions.
#

#
# SoC-specific compile-time definitions.
#

BOARD_SOC_TYPE := IMX6DQ
BOARD_HAVE_VPU := true
HAVE_FSL_IMX_GPU2D := true
HAVE_FSL_IMX_GPU3D := true
HAVE_FSL_IMX_IPU := true
BOARD_KERNEL_BASE := 0x14000000
LOAD_KERNEL_ENTRY := 0x10008000

-include external/fsl_vpu_omx/codec_env.mk
-include external/fsl_imx_omx/codec_env.mk

TARGET_HAVE_IMX_GRALLOC := true
TARGET_HAVE_IMX_HWCOMPOSER = true
TARGET_HAVE_VIV_HWCOMPOSER = true
USE_OPENGL_RENDERER := true
TARGET_CPU_SMP := true

export BUILD_ID?=2.1.0-ga-rc2
export BUILD_NUMBER?=20170605

#
# Product-specific compile-time definitions.
#

TARGET_BOARD_PLATFORM := imx6
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_VARIANT := cortex-a9
ARCH_ARM_HAVE_TLS_REGISTER := true

TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := false
TARGET_NO_RECOVERY := false
TARGET_NO_RADIOIMAGE := true

TARGET_PROVIDES_INIT_RC := true

BOARD_SOC_CLASS := IMX6

#BOARD_USES_GENERIC_AUDIO := true
BOARD_USES_ALSA_AUDIO := true
BUILD_WITH_ALSA_UTILS := true
BOARD_HAVE_BLUETOOTH := true
USE_CAMERA_STUB := false
BOARD_CAMERA_LIBRARIES := libcamera

BOARD_HAVE_WIFI := true

BOARD_NOT_HAVE_MODEM := false
BOARD_MODEM_VENDOR := HUAWEI
BOARD_MODEM_ID := EM750M
BOARD_MODEM_HAVE_DATA_DEVICE := true
BOARD_HAVE_IMX_CAMERA := true
BOARD_HAVE_USB_CAMERA := true

BUILD_WITHOUT_FSL_DIRECTRENDER := false
BUILD_WITHOUT_FSL_XEC := true

TARGET_USERIMAGES_BLOCKS := 204800

BUILD_WITH_GST := false

# Enable dex-preoptimization to speed up first boot sequence
ifeq ($(HOST_OS),linux)
   ifeq ($(TARGET_BUILD_VARIANT),user)
	ifeq ($(WITH_DEXPREOPT),)
	    WITH_DEXPREOPT := true
	endif
   endif
endif

# for ums config, only export one partion instead of the whole disk
UMS_ONEPARTITION_PER_DISK := true

PREBUILT_FSL_IMX_CODEC := true
PREBUILT_FSL_IMX_GPU := true
PREBUILT_FSL_WFDSINK := true
PREBUILT_FSL_HWCOMPOSER := true

# override some prebuilt setting if DISABLE_FSL_PREBUILT is define
ifeq ($(DISABLE_FSL_PREBUILT),GPU)
PREBUILT_FSL_IMX_GPU := false
else ifeq ($(DISABLE_FSL_PREBUILT),WFD)
PREBUILT_FSL_WFDSINK := false
else ifeq ($(DISABLE_FSL_PREBUILT),HWC)
PREBUILT_FSL_HWCOMPOSER := false
else ifeq ($(DISABLE_FSL_PREBUILT),ALL)
PREBUILT_FSL_IMX_GPU := false
PREBUILT_FSL_WFDSINK := false
PREBUILT_FSL_HWCOMPOSER := false
endif

# use non-neon memory copy on mx6x to get better performance
ARCH_ARM_USE_NON_NEON_MEMCPY := true

# for kernel/user space split
# comment out for 1g/3g space split
# TARGET_KERNEL_2G := true

BOARD_BOOTIMAGE_PARTITION_SIZE := 16777216
BOARD_RECOVERYIMAGE_PARTITION_SIZE := 16777216

BOARD_SYSTEMIMAGE_PARTITION_SIZE := 629145600
BOARD_CACHEIMAGE_PARTITION_SIZE := 444596224
BOARD_FLASH_BLOCK_SIZE := 4096
TARGET_RECOVERY_UI_LIB := librecovery_ui_imx

# Freescale multimedia parser related prop setting
# Define fsl avi/aac/asf/mkv/flv/flac format support
ADDITIONAL_BUILD_PROPERTIES += \
    ro.FSL_AVI_PARSER=1 \
    ro.FSL_AAC_PARSER=1 \
    ro.FSL_ASF_PARSER=0 \
    ro.FSL_FLV_PARSER=1 \
    ro.FSL_MKV_PARSER=1 \
    ro.FSL_FLAC_PARSER=1 \
    ro.FSL_MPG2_PARSER=1 \
    ro.FSL_REAL_PARSER=0 \

-include device/google/gapps/gapps_config.mk

include device/fsl-proprietary/gpu-viv/fsl-gpu.mk

# sabresd_6dq default target for EXT4
BUILD_TARGET_FS ?= ext4
TARGET_USERIMAGES_USE_EXT4 := true
TARGET_USERIMAGES_USE_UBIFS := false

ifeq ($(BUILD_TARGET_DEVICE),sd)

ADDITIONAL_BUILD_PROPERTIES += \
	ro.boot.storage_type=sd

TARGET_RECOVERY_FSTAB = device/rtx/a6_imx6dq/etc/fstab.sd.retronix

# build for ext4
PRODUCT_COPY_FILES +=	\
	device/rtx/a6_imx6dq/etc/fstab.sd.retronix:root/fstab.retronix

else

ADDITIONAL_BUILD_PROPERTIES += \
	ro.boot.storage_type=emmc

TARGET_RECOVERY_FSTAB = device/rtx/a6_imx6dq/etc/fstab.emmc.retronix

# build for ext4
PRODUCT_COPY_FILES +=	\
	device/rtx/a6_imx6dq/etc/fstab.emmc.retronix:root/fstab.retronix

endif # BUILD_TARGET_DEVICE

TARGET_BOOTLOADER_BOARD_NAME := A6
PRODUCT_MODEL := A6-MX6DQ

TARGET_RELEASETOOLS_EXTENSIONS := device/rtx/a6_imx6dq

# UNITE is a virtual device.
BOARD_WLAN_DEVICE            := UNITE
WPA_SUPPLICANT_VERSION       := VER_0_8_UNITE

BOARD_WPA_SUPPLICANT_DRIVER  := NL80211
BOARD_HOSTAPD_DRIVER         := NL80211

BOARD_HOSTAPD_PRIVATE_LIB_BCM               := lib_driver_cmd_bcmdhd
BOARD_WPA_SUPPLICANT_PRIVATE_LIB_BCM        := lib_driver_cmd_bcmdhd

BOARD_SUPPORT_BCM_WIFI  := true
#for intel vendor
ifeq ($(BOARD_WLAN_VENDOR),INTEL)
BOARD_HOSTAPD_PRIVATE_LIB                := private_lib_driver_cmd
BOARD_WPA_SUPPLICANT_PRIVATE_LIB         := private_lib_driver_cmd
WPA_SUPPLICANT_VERSION                   := VER_0_8_X
HOSTAPD_VERSION                          := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB         := private_lib_driver_cmd_intel
WIFI_DRIVER_MODULE_PATH                  := "/system/lib/modules/iwlagn.ko"
WIFI_DRIVER_MODULE_NAME                  := "iwlagn"
WIFI_DRIVER_MODULE_PATH                  ?= auto
endif

WIFI_DRIVER_FW_PATH_STA        := "/system/etc/firmware/bcm/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_P2P        := "/system/etc/firmware/bcm/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_AP         := "/system/etc/firmware/bcm/fw_bcmdhd_apsta.bin"
WIFI_DRIVER_FW_PATH_PARAM      := "/sys/module/bcmdhd/parameters/firmware_path"

BOARD_MODEM_VENDOR := AMAZON

USE_ATHR_GPS_HARDWARE := true
USE_QEMU_GPS_HARDWARE := false

#for accelerator sensor, need to define sensor type here
BOARD_HAS_SENSOR := true
SENSOR_MMA8451 := true

# for recovery service
TARGET_SELECT_KEY := 28

# we don't support sparse image.
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := false
DM_VERITY_RUNTIME_CONFIG := true
# uncomment below lins if use NAND
#TARGET_USERIMAGES_USE_UBIFS = true


ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
UBI_ROOT_INI := device/rtx/a6_imx6dq/ubi/ubinize.ini
TARGET_MKUBIFS_ARGS := -m 4096 -e 516096 -c 4096 -x none
TARGET_UBIRAW_ARGS := -m 4096 -p 512KiB $(UBI_ROOT_INI)
endif

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
ifeq ($(TARGET_USERIMAGES_USE_EXT4),true)
$(error "TARGET_USERIMAGES_USE_UBIFS and TARGET_USERIMAGES_USE_EXT4 config open in same time, please only choose one target file system image")
endif
endif

BOARD_KERNEL_CMDLINE := console=ttymxc0,115200 init=/init video=mxcfb0:dev=ldb,bpp=32 video=mxcfb1:off video=mxcfb2:off video=mxcfb3:off vmalloc=256M androidboot.console=ttymxc0 consoleblank=0 androidboot.hardware=freescale cma=384M

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
#UBI boot command line.
# Note: this NAND partition table must align with MFGTool's config.
BOARD_KERNEL_CMDLINE +=  mtdparts=gpmi-nand:16m(bootloader),16m(bootimg),128m(recovery),-(root) gpmi_debug_init ubi.mtd=3
endif


# Broadcom BCM4339 BT
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := device/rtx/a6_imx6dq/bluetooth

USE_ION_ALLOCATOR := false
USE_GPU_ALLOCATOR := true

# camera hal v3
IMX_CAMERA_HAL_V3 := true

#define consumer IR HAL support
IMX6_CONSUMER_IR_HAL := false

TARGET_BOOTLOADER_CONFIG := ${UBOOT_CFG}
TARGET_KERNEL_DEFCONF := ${KERNEL_CFG}
TARGET_BOARD_DTS_CONFIG := ${KERNEL_DTB}

BOARD_SEPOLICY_DIRS := \
       device/rtx/a6_imx6dq/sepolicy 

