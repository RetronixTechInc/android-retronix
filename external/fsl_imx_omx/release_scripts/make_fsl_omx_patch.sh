#!/bin/bash

echo arg1 is "$1"
if [ "$1" = "" ];then
    echo "Please input package version like: kk4.2.2_1.0.0-alpha"
    exit -1
fi

TARGET_DIR=fsl_vpu_omx
INFO_PATH=imx-release-info
VERSION=`echo $1 | tr "[:lower:]" "[:upper:]"`

# to android root
cd ../../../

# in case current mode is core mode, switch to full mode
if [ -f device/fsl-codec/fsl-codec.mk.bak ];then
    mv device/fsl-codec/fsl-codec.mk.bak device/fsl-codec/fsl-codec.mk
fi
sed -i 's/HAVE_FSL_IMX_CODEC := false/HAVE_FSL_IMX_CODEC := true/g' external/fsl_imx_omx/codec_env.mk

cd external/

if [ -d ${TARGET_DIR} ];then
    echo "removing old fsl_vpu_omx..."
    rm -rf ${TARGET_DIR}
fi

echo "creating $TARGET_DIR"
sleep 1

mkdir $TARGET_DIR
mkdir -p $TARGET_DIR/OpenMAXIL/src/component
mkdir -p $TARGET_DIR/OpenMAXIL/release/registry
mkdir -p $TARGET_DIR/OpenMAXIL/test


cp -af fsl_imx_omx/OSAL         $TARGET_DIR
cp -af fsl_imx_omx/stagefright  $TARGET_DIR
cp -af fsl_imx_omx/utils        $TARGET_DIR
cp fsl_imx_omx/Android_vpu.mk   $TARGET_DIR/Android.mk
cp fsl_imx_omx/codec_env.mk     $TARGET_DIR/
cp -af fsl_imx_omx/OpenMAXIL/src/component/common       $TARGET_DIR/OpenMAXIL/src/component
cp -af fsl_imx_omx/OpenMAXIL/src/component/vpu_dec_v2   $TARGET_DIR/OpenMAXIL/src/component
cp -af fsl_imx_omx/OpenMAXIL/src/component/vpu_enc      $TARGET_DIR/OpenMAXIL/src/component
cp -af fsl_imx_omx/OpenMAXIL/src/component/vpu_wrapper  $TARGET_DIR/OpenMAXIL/src/component
cp -af fsl_imx_omx/OpenMAXIL/src/resource_mgr           $TARGET_DIR/OpenMAXIL/src
cp -af fsl_imx_omx/OpenMAXIL/src/core                   $TARGET_DIR/OpenMAXIL/src
cp -af fsl_imx_omx/OpenMAXIL/release/registry/*         $TARGET_DIR/OpenMAXIL/release/registry/
cp -af fsl_imx_omx/OpenMAXIL/ghdr                       $TARGET_DIR/OpenMAXIL/
cp -af fsl_imx_omx/OpenMAXIL/test/vpu_test              $TARGET_DIR/OpenMAXIL/test/
cp ../device/fsl-codec/ghdr/common/*.h      $TARGET_DIR/OpenMAXIL/src/component/common/

sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OSAL/linux/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/utils/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/utils/id3_parser/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/component/common/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/component/vpu_dec_v2/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/component/vpu_enc/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/component/vpu_wrapper/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/resource_mgr/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/src/core/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/release/registry/Android.mk
sed -i 's/ifeq ($(HAVE_FSL_IMX_CODEC),true)/ifeq ($(HAVE_FSL_IMX_CODEC),false)/g' $TARGET_DIR/OpenMAXIL/test/vpu_test/Android.mk
sed -i 's/HAVE_FSL_IMX_CODEC := true/HAVE_FSL_IMX_CODEC := false/g' $TARGET_DIR/codec_env.mk


echo "=== Copy package and script file to root directory ==="
sleep 1

cd ../
cp external/fsl_imx_omx/release_scripts/clean_obj_before_building.sh .
cp external/fsl_imx_omx/release_scripts/switch_build_to.sh .

##### get EULA from info git

if [ -d $INFO_PATH ]; then
    rm -rf $INFO_PATH
fi

git clone http://sw-stash.freescale.net/scm/imx/imx-release-info.git

if [ ! -d $INFO_PATH ]; then
    echo "get imx-release-info git fail!"
    exit 127
fi

cp $INFO_PATH/EULA.txt .
cp $INFO_PATH/android/release/SCR-omxplayer.txt .

sed -i "s/Package:.*/Package:                   android_${VERSION}_omxplayer_source.tar/g" SCR-omxplayer.txt 

DATE=`date +%Y-%b-%d`
sed -i "s/Date Created.*/Date Created:              $DATE/g" SCR-omxplayer.txt

rm -rf $INFO_PATH

cp external/fsl_imx_omx/release_scripts/omxplayer_package_manifest.txt .

##### make tar file

tar czf android_${VERSION}_omxplayer_source.tar.gz external/fsl_imx_omx device/fsl-codec clean_obj_before_building.sh switch_build_to.sh EULA.txt SCR-omxplayer.txt omxplayer_package_manifest.txt

rm -rf external/fsl_imx_omx
rm -rf device/fsl-codec

echo "=== finished ==="
sleep 1
