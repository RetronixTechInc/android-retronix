# This is a FSL Android Reference Design platform based on i.MX6Q ARD board
# It will inherit from FSL core product which in turn inherit from Google generic

$(call inherit-product, device/fsl/imx6/imx6.mk)
$(call inherit-product-if-exists,vendor/google/products/gms.mk)

ifneq ($(wildcard device/fsl/sabresd_6sx/fstab_nand.freescale),)
$(shell touch device/fsl/sabresd_6sx/fstab_nand.freescale)
endif

ifneq ($(wildcard device/fsl/sabresd_6sx/fstab.freescale),)
$(shell touch device/fsl/sabresd_6sx/fstab.freescale)
endif

# Overrides
PRODUCT_NAME := sabresd_6sx
PRODUCT_DEVICE := sabresd_6sx

PRODUCT_COPY_FILES += \
	device/fsl/sabresd_6sx/init.rc:root/init.freescale.rc \
	device/fsl/sabresd_6sx/audio_policy.conf:system/etc/audio_policy.conf \
	device/fsl/sabresd_6sx/audio_effects.conf:system/vendor/etc/audio_effects.conf
# setup dm-verity configs.
 PRODUCT_SYSTEM_VERITY_PARTITION := /dev/block/mmcblk3p5
 $(call inherit-product, build/target/product/verity.mk)

# GPU files

DEVICE_PACKAGE_OVERLAYS := device/fsl/sabresd_6sx/overlay

PRODUCT_CHARACTERISTICS := tablet

PRODUCT_AAPT_CONFIG += xlarge large tvdpi hdpi xhdpi

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/android.hardware.camera.xml:system/etc/permissions/android.hardware.camera.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.faketouch.xml:system/etc/permissions/android.hardware.faketouch.xml \
	frameworks/native/data/etc/android.software.sip.voip.xml:system/etc/permissions/android.software.sip.voip.xml \
	frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
	frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
	device/fsl/sabresd_6sx/required_hardware.xml:system/etc/permissions/required_hardware.xml
