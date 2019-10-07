#
# SoC-specific compile-time definitions.
#

BOARD_SOC_TYPE := IMX8MM
BOARD_TYPE := EVK
BOARD_HAVE_VPU := true
BOARD_VPU_TYPE := hantro
HAVE_FSL_IMX_GPU2D := true
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

#
# Product-specific compile-time definitions.
#

IMX_DEVICE_PATH := device/fsl/imx8m/evk_8mm

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

PRODUCT_MODEL := EVK_8MM

TARGET_BOOTLOADER_POSTFIX := bin

USE_OPENGL_RENDERER := true
TARGET_CPU_SMP := true

BOARD_WLAN_DEVICE_UNITE      := UNITE
WPA_SUPPLICANT_VERSION       := VER_0_8_X
BOARD_WPA_SUPPLICANT_DRIVER  := NL80211
BOARD_HOSTAPD_DRIVER         := NL80211

# In UNITE mode,Use default macro for bcmdhd and use unite macro for qcom
ifeq ($(BOARD_WLAN_DEVICE_UNITE), UNITE)
BOARD_WLAN_DEVICE            := bcmdhd
BOARD_HOSTAPD_PRIVATE_LIB_QCA           := lib_driver_cmd_qcwcn
BOARD_WPA_SUPPLICANT_PRIVATE_LIB_QCA    := lib_driver_cmd_qcwcn
BOARD_HOSTAPD_PRIVATE_LIB_BCM           := lib_driver_cmd_bcmdhd
BOARD_WPA_SUPPLICANT_PRIVATE_LIB_BCM    := lib_driver_cmd_bcmdhd
endif

WIFI_DRIVER_FW_PATH_PARAM := "/sys/module/brcmfmac/parameters/alternative_fw_path"

# QCA wifi support dual interface
WIFI_HIDL_FEATURE_DUAL_INTERFACE := true

# QCA qcacld wifi driver module
BOARD_VENDOR_KERNEL_MODULES += \
    $(KERNEL_OUT)/drivers/net/wireless/qcacld-2.0/wlan.ko

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

# Qcom 1PJ(QCA9377) BT
BOARD_HAVE_BLUETOOTH_QCOM := true
BOARD_HAS_QCA_BT_ROME := true
BOARD_HAVE_BLUETOOTH_BLUEZ := false
QCOM_BT_USE_SIBS := true
ifeq ($(QCOM_BT_USE_SIBS), true)
    WCNSS_FILTER_USES_SIBS := true
endif

# BCM 1MW BT
BOARD_HAVE_BLUETOOTH_BCM := true

UBOOT_POST_PROCESS := true

# camera hal v3
IMX_CAMERA_HAL_V3 := true

BOARD_HAVE_USB_CAMERA := true

USE_ION_ALLOCATOR := true
USE_GPU_ALLOCATOR := false

BOARD_AVB_ENABLE := true
BOARD_AVB_ALGORITHM := SHA256_RSA4096
# The testkey_rsa4096.pem is copied from external/avb/test/data/testkey_rsa4096.pem
BOARD_AVB_KEY_PATH := device/fsl/common/security/testkey_rsa4096.pem
TARGET_USES_MKE2FS := true

# define frame buffer count
NUM_FRAMEBUFFER_SURFACE_BUFFERS := 3

ifeq ($(PRODUCT_IMX_DRM),true)
CMASIZE=736M
else
CMASIZE=800M
endif

KERNEL_NAME := Image
ifeq ($(LOW_MEMORY),true)
BOARD_KERNEL_CMDLINE := init=/init androidboot.console=ttymxc1 androidboot.hardware=freescale cma=320M@0x400M-0xb80M androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never loop.max_part=7 androidboot.displaymode=720p galcore.contiguousSize=33554432
else
BOARD_KERNEL_CMDLINE := init=/init androidboot.console=ttymxc1 androidboot.hardware=freescale cma=$(CMASIZE)@0x400M-0xb80M androidboot.primary_display=imx-drm firmware_class.path=/vendor/firmware transparent_hugepage=never loop.max_part=7
endif

# Default wificountrycode
BOARD_KERNEL_CMDLINE += androidboot.wificountrycode=CN

# Defaultly evk_8mm use QCA 1PJ wifi module, if use BCM 1MW module, set androidboot.wifivendor=bcm
BOARD_KERNEL_CMDLINE += androidboot.wifivendor=qca

ifeq ($(TARGET_USERIMAGES_USE_UBIFS),true)
ifeq ($(TARGET_USERIMAGES_USE_EXT4),true)
$(error "TARGET_USERIMAGES_USE_UBIFS and TARGET_USERIMAGES_USE_EXT4 config open in same time, please only choose one target file system image")
endif
endif

BOARD_PREBUILT_DTBOIMAGE := out/target/product/evk_8mm/dtbo-imx8mm.img
# imx8mm with MIPI-HDMI display and QCA wifi and Trusty support
TARGET_BOARD_DTS_CONFIG ?= imx8mm:fsl-imx8mm-trusty-evk.dtb
# u-boot target for imx8mm_evk with LPDDR4 on board
ifeq ($(LOW_MEMORY),true)
TARGET_BOOTLOADER_CONFIG := imx8mm:imx8mm_evk_1g_ddr_android_defconfig
else
TARGET_BOOTLOADER_CONFIG := imx8mm:imx8mm_evk_android_defconfig
endif
# u-boot target for imx8mm_evk with DDR4 on board
TARGET_BOOTLOADER_CONFIG += imx8mm-ddr4:imx8mm_ddr4_evk_android_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mm-trusty:imx8mm_evk_android_trusty_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mm-trusty-4g:imx8mm_evk_4g_android_trusty_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mm-4g:imx8mm_evk_4g_android_defconfig
# imx8mm with MIPI panel display and QCA wifi
TARGET_BOARD_DTS_CONFIG += imx8mm-mipi-panel:fsl-imx8mm-evk-rm67191.dtb
# imx8mm with MIPI-HDMI display, QCA wifi and m4 image to support LPA
TARGET_BOARD_DTS_CONFIG += imx8mm-m4:fsl-imx8mm-evk-m4.dtb
# imx8mm with MIPI-HDMI display with BCM wifi
TARGET_BOARD_DTS_CONFIG += imx8mm-ddr4:fsl-imx8mm-ddr4-evk.dtb
TARGET_KERNEL_DEFCONFIG := android_defconfig

TARGET_KERNEL_ADDITION_DEFCONF := android_addition_defconfig

# u-boot target used by uuu for imx8mm_evk with LPDDR4 on board
TARGET_BOOTLOADER_CONFIG += imx8mm-evk-uuu:imx8mm_evk_android_uuu_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mm-4g-evk-uuu:imx8mm_evk_4g_android_uuu_defconfig
TARGET_BOOTLOADER_CONFIG += imx8mm-trusty-4g-evk-uuu:imx8mm_evk_4g_android_uuu_defconfig
# u-boot target used by uuu for imx8mm_evk with DDR4 on board
TARGET_BOOTLOADER_CONFIG += imx8mm-ddr4-evk-uuu:imx8mm_ddr4_evk_android_uuu_defconfig

BOARD_SEPOLICY_DIRS := \
       device/fsl/imx8m/sepolicy \
       $(IMX_DEVICE_PATH)/sepolicy

ifeq ($(PRODUCT_IMX_DRM),true)
BOARD_SEPOLICY_DIRS += \
       $(IMX_DEVICE_PATH)/sepolicy_drm
endif

TARGET_BOARD_KERNEL_HEADERS := device/fsl/common/kernel-headers
