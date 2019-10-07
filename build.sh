#! /bin/bash

ANDROID_SRC_ROOT=$(pwd)

BRAN=RTX_NXP_Android900
MANIFEST=RTX_NXP_Android900.xml

repo init -u ssh://git@github.com/RetronixTechInc/android-manifests.git -b ${BRAN} -m ${MANIFEST}

repo sync

export  MY_ANDROID=$ANDROID_SRC_ROOT
cd ${ANDROID_SRC_ROOT}; 

source $ANDROID_SRC_ROOT/build/envsetup.sh 
lunch evk_8mm-userdebug
make 2>&1 | tee build-log.txt

