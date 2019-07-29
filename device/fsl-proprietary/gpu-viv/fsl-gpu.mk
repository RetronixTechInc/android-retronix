ifeq ($(PREBUILT_FSL_IMX_GPU),true)
ifeq ($(HAVE_FSL_IMX_GPU2D),true)
ifeq ($(HAVE_FSL_IMX_GPU3D),true)
PRODUCT_COPY_FILES += \
        device/fsl-proprietary/gpu-viv/lib/egl/egl.cfg:system/lib/egl/egl.cfg
PRODUCT_COPY_FILES += \
        device/fsl-proprietary/gpu-viv/lib/egl/libEGL_VIVANTE.so:system/lib/egl/libEGL_VIVANTE.so \
        device/fsl-proprietary/gpu-viv/lib/egl/libGLESv1_CM_VIVANTE.so:system/lib/egl/libGLESv1_CM_VIVANTE.so \
        device/fsl-proprietary/gpu-viv/lib/egl/libGLESv2_VIVANTE.so:system/lib/egl/libGLESv2_VIVANTE.so \
        device/fsl-proprietary/gpu-viv/lib/libGAL.so:system/lib/libGAL.so \
        device/fsl-proprietary/gpu-viv/lib/libGAL.so:obj/lib/libGAL.so \
        device/fsl-proprietary/gpu-viv/lib/libg2d.so:system/lib/libg2d.so \
        device/fsl-proprietary/gpu-viv/lib/libg2d.so:obj/lib/libg2d.so \
        device/fsl-proprietary/gpu-viv/lib/libgpuhelper.so:system/lib/libgpuhelper.so \
        device/fsl-proprietary/gpu-viv/lib/libgpuhelper.so:obj/lib/libgpuhelper.so \
        device/fsl-proprietary/gpu-viv/lib/libGLSLC.so:system/lib/libGLSLC.so \
        device/fsl-proprietary/gpu-viv/lib/libVSC.so:system/lib/libVSC.so \
        device/fsl-proprietary/gpu-viv/lib/hw/gralloc_viv.imx6.so:system/lib/hw/gralloc_viv.imx6.so
else
PRODUCT_COPY_FILES += \
        device/fsl-proprietary/gpu-viv/lib/libGAL.so:obj/lib/libGAL.so \
        device/fsl-proprietary/gpu-viv/lib/libGAL.so:system/lib/libGAL.so \
        device/fsl-proprietary/gpu-viv/lib/libg2d.so:obj/lib/libg2d.so \
        device/fsl-proprietary/gpu-viv/lib/libg2d.so:system/lib/libg2d.so \
        device/fsl-proprietary/gpu-viv/lib/libgpuhelper.so:system/lib/libgpuhelper.so \
        device/fsl-proprietary/gpu-viv/lib/libgpuhelper.so:obj/lib/libgpuhelper.so \
        device/fsl-proprietary/gpu-viv/lib/hw/gralloc_viv.imx6.so:system/lib/hw/gralloc_viv.imx6.so
endif
endif
ifeq ($(TARGET_HAVE_VIV_HWCOMPOSER),true)
PRODUCT_COPY_FILES += \
        device/fsl-proprietary/gpu-viv/lib/hw/hwcomposer_viv.imx6.so:system/lib/hw/hwcomposer_viv.imx6.so
endif
endif
