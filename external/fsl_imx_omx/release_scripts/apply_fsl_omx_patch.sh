#!/bin/bash

echo "=== modify config files ==="

sed -i 's/HAVE_FSL_IMX_CODEC := false/HAVE_FSL_IMX_CODEC := true/g' device/fsl/imx6/soc/imx6dq.mk
sed -i 's/HAVE_FSL_IMX_CODEC := false/HAVE_FSL_IMX_CODEC := true/g' device/fsl/imx5x/BoardConfigCommon.mk
sed -i 's/PREBUILT_FSL_IMX_CODEC := false/PREBUILT_FSL_IMX_CODEC := true/g' device/fsl/imx6/BoardConfigCommon.mk
sed -i 's/PREBUILT_FSL_IMX_CODEC := false/PREBUILT_FSL_IMX_CODEC := true/g' device/fsl/imx5x/BoardConfigCommon.mk

echo "=== apply patch finished ==="

