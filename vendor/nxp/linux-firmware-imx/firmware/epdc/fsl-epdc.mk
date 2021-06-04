ifeq ($(LINUX_FIRMWARE_IMX_PATH),)
     LINUX_FIRMWARE_IMX_PATH := external
endif
PRODUCT_COPY_FILES += \
        $(LINUX_FIRMWARE_IMX_PATH)/linux-firmware-imx/firmware/epdc/epdc_ED060XH2C1.fw.nonrestricted:root/lib/firmware/imx/epdc/epdc_ED060XH2C1.fw
