#!/bin/bash

if [ "$1" = "--help" ] || [ "$1" = "-h" ];then
    echo "Usage:"
    echo "./switch_build_to.sh"
    echo "        -- get current build mode"
    echo "./switch_build_to.sh core"
    echo "        -- switch to core mode"
    echo "./switch_build_to.sh full"
    echo "        -- switch to full mode"
    exit 0
fi

BUILD="$1"

if [ "${BUILD}" = "" ];then
    if [ ! -f external/fsl_imx_omx/codec_env.mk ] && [ ! -f device/fsl-codec/fsl-codec.mk ];then
        echo ""
        echo "Current build mode is core."
        echo ""
        exit 0
    fi

    grep -q "HAVE_FSL_IMX_CODEC := true" external/fsl_imx_omx/codec_env.mk
    if [ "$?" = 0 ] && [ -f device/fsl-codec/fsl-codec.mk ];then
        echo ""
        echo "Current build mode is full."
        echo ""
        exit 0
    fi

    grep -q "HAVE_FSL_IMX_CODEC := false" external/fsl_imx_omx/codec_env.mk
    if [ "$?" = 0 ] && [ ! -f device/fsl-codec/fsl-codec.mk ];then
        echo ""
        echo "Current build mode is core."
        echo ""
        exit 0
    fi

    echo ""
    echo "Error: current build mode is uncertain."
    echo "Please check external/fsl_imx_omx/codec_env.mk and device/fsl-codec/fsl-codec.mk ."
    echo ""
    exit 0
fi

if [ "${BUILD}" != "core" ] && [ "${BUILD}" != "full" ];then
    echo ""
    echo "Please input build type: core or full"
    echo ""
    exit -1
fi

if [ "${BUILD}" = "core" ];then
    if [ -f external/fsl_imx_omx/codec_env.mk ];then
        sed -i 's/HAVE_FSL_IMX_CODEC := true/HAVE_FSL_IMX_CODEC := false/g' external/fsl_imx_omx/codec_env.mk
    fi
    if [ -f device/fsl-codec/fsl-codec.mk ];then
        mv device/fsl-codec/fsl-codec.mk device/fsl-codec/fsl-codec.mk.bak
    fi
fi

if [ "${BUILD}" = "full" ];then
    if [ ! -d external/fsl_imx_omx ];then
        echo ""
        echo "Error: Can't switch to full mode because external/fsl_imx_omx not exist!"
        echo "    Check whether omx patch applied or not."
        echo ""
        exit -1
    fi
    if [ ! -d device/fsl-codec ];then
        echo ""
        echo "Error: Can't switch to full mode because device/fsl-codec not exist!"
        echo "    Check whether omx patch applied or not."
        echo ""
        exit -1
    fi
    sed -i 's/HAVE_FSL_IMX_CODEC := false/HAVE_FSL_IMX_CODEC := true/g' external/fsl_imx_omx/codec_env.mk
    if [ -f device/fsl-codec/fsl-codec.mk.bak ];then
        mv device/fsl-codec/fsl-codec.mk.bak device/fsl-codec/fsl-codec.mk
    fi
fi

echo "Finished"
