#
# SoC-specific compile-time definitions.
#

BOARD_SOC_TYPE := IMX8MN
BOARD_TYPE := EVK
BOARD_HAVE_VPU := false
HAVE_FSL_IMX_GPU2D := false
HAVE_FSL_IMX_GPU3D := true
HAVE_FSL_IMX_IPU := false
HAVE_FSL_IMX_PXP := false
BOARD_KERNEL_BASE := 0x40400000
TARGET_GRALLOC_VERSION := v3
TARGET_HIGH_PERFORMANCE := true
TARGET_USES_HWC2 := true
TARGET_HWCOMPOSER_VERSION := v2.0
TARGET_HAVE_VIV_HWCOMPOSER = true
TARGET_FSL_IMX_2D := GPU2D
USE_OPENGL_RENDERER := true
TARGET_CPU_SMP := true
TARGET_HAVE_VULKAN := true
ENABLE_CFI=false

# enable opencl 2d.
TARGET_OPENCL_2D := true

#
# Product-specific compile-time definitions.
#

IMX_DEVICE_PATH := device/fsl/imx8m/evk_8mn

include $(IMX_DEVICE_PATH)/build_id.mk
include device/fsl/imx8m/BoardConfigCommon.mk
ifeq ($(PREBUILT_FSL_IMX_CODEC),true)
-include $(FSL_CODEC_PATH)/fsl-codec/fsl-codec.mk
endif

BUILD_TARGET_FS ?= ext4
TARGET_USERIMAGES_USE_EXT4 := true

TARGET_RECOVERY_FSTAB = $(IMX_DEVICE_PATH)/fstab.freescale

# Support gpt
ifeq ($(LOW_MEMORY),true)
BOARD_BPT_INPUT_FILES += device/fsl/common/partition/device-partitions-7GB-ab.bpt
ADDITION_BPT_PARTITION = partition-table-13GB:device/fsl/common/partition/device-partitions-13GB-ab.bpt \
                         partition-table-28GB:device/fsl/common/partition/device-partitions-28GB-ab.bpt
else
BOARD_BPT_INPUT_FILES += device/fsl/common/partition/device-partitions-13GB-ab.bpt
ADDITION_BPT_PARTITION = partition-table-7GB:device/fsl/common/partition/device-partitions-7GB-ab.bpt \
                         partition-table-28GB:device/fsl/common/partition/device-partitions-28GB-ab.bpt
endif


# Vendor Interface manifest and compatibility
DEVICE_MANIFEST_FILE := $(IMX_DEVICE_PATH)/manifest.xml
DEVICE_MATRIX_FILE := $(IMX_DEVICE_PATH)/compatibility_matrix.xml

TARGET_BOOTLOADER_BOARD_NAME := EVK

PRODUCT_MODEL := EVK_8MN

TARGET_BOOTLOADER_POSTFIX := bin

USE_OPENGL_RENDERER := true
TARGET_CPU_SMP := true

BOARD_WLAN_DEVICE            := bcmdhd
WPA_SUPPLICANT_VERSION       := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER  := NL80211
BOARD_HOSTAPD_DRIVER         := NL80211
BOARD_HOSTAPD_PRIVATE_LIB               := lib_driver_cmd_$(BOARD_WLAN_DEVICE)
BOARD_WPA_SUPPLICANT_PRIVATE_LIB        := lib_driver_cmd_$(BOARD_WLAN_DEVICE)

WIFI_DRIVER_FW_PATH_PARAM := "/sys/module/brcmfmac/parameters/alternative_fw_path"

# BCM fmac wifi driver module
BOARD_VENDOR_KERNEL_MODULES += \
    $(KERNEL_OUT)/drivers/net/wireless/broadcom/brcm80211/brcmfmac/brcmfmac.ko \
    $(KERNEL_OUT)/drivers/net/wireless/broadcom/brcm80211/brcmutil/brcmutil.ko

BOARD_USE_SENSOR_FUSION := true

# for recovery service
TARGET_SELECT_KEY := 28
# we don't support sparse image.
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := false

BOARD_BLUETOOTH_BDROID_BUILDCFG_INCLUDE_DIR := $(IMX_DEVICE_PATH)/bluetooth

# BCM 1MW BT
BOARD_HAVE_BLUETOOTH_BCM := true

UBOOT_POST_PROCESS := true

# camera hal v3
IMX_CAMERA_HAL_V3 := true

BOARD_HAVE_USB_CAMERA := true

# whether to accelerate camera service with openCL
# it will make camera service load the opencl lib in vendor
# and break the full treble rule
#OPENCL_2D_IN_CAMERA := true

USE_ION_ALLOCATOR := true
USE_GPU_ALLOCATOR := false

BOARD_AVB_ENABLE := true
BOARD_AVB_ALGORITHM := SHA256_RSA4096
# The testkey_rsa4096.pem is copied from external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_KEY_PATH := device/fsl/common/security/testkey_rsa4096.pem
TARGET_USES_MKE2FS := true

# define frame buffer count
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3

CMASIZE=800M

KERNEL_NAME := Image
ifeq ($(LOW_MEMORY),true)
BOARD_KERNEL_CMDLINE := init=/init androidboot.console=ttymxc1 androidboot.hardware=freescale cma=320M@0x400M-0xb80M androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never loop.max_part=7 androidboot.displaymode=720p galcore.contiguousSize=33554432
else
BOARD_KERNEL_CMDLINE := init=/init androidboot.console=ttymxc1 androidboot.hardware=freescale cma=$(CMASIZE)@0x400M-0xb80M androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never loop.max_part=7
endif

# Default wificountrycode
BOARD_KERNEL_CMDLINE += androidboot.wificountrycode=CN

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
ifeq ($(TARGET_USERIMAGES_USE_EXT4),true)
$(error "TARGET_USERIMAGES_USE_UBIFS and TARGET_USERIMAGES_USE_EXT4 config open in same time, please only choose one target file system image")
endif
endif

BOARD_PREBUILT_DTBOIMAGE := out/target/product/evk_8mn/dtbo-imx8mn.img

# u-boot target for stand config and Trusty OS config
TARGET_BOOTLOADER_CONFIG := imx8mn:imx8mn_ddr4_evk_android_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mn-trusty:imx8mn_ddr4_evk_android_trusty_defconfig
# u-boot target used by uuu for imx8mn_evk
TARGET_BOOTLOADER_CONFIG += imx8mn-evk-uuu:imx8mn_ddr4_evk_android_uuu_defconfig
# imx8mn with MIPI-HDMI display, BCM wifi and support trusty
TARGET_BOARD_DTS_CONFIG := imx8mn:fsl-imx8mn-ddr4-evk-trusty.dtb
# imx8mn with MIPI panel display and BCM wifi
TARGET_BOARD_DTS_CONFIG += imx8mn-mipi-panel:fsl-imx8mn-ddr4-evk-rm67191.dtb
# imx8mn with MIPI-HDMI display and BCM wifi and M7 image
TARGET_BOARD_DTS_CONFIG += imx8mn-rpmsg:fsl-imx8mn-ddr4-evk-rpmsg.dtb


TARGET_KERNEL_DEFCONFIG := android_defconfig

TARGET_KERNEL_ADDITION_DEFCONF := android_addition_defconfig

BOARD_SEPOLICY_DIRS := \
       device/fsl/imx8m/sepolicy \
       $(IMX_DEVICE_PATH)/sepolicy

TARGET_BOARD_KERNEL_HEADERS := device/fsl/common/kernel-headers
