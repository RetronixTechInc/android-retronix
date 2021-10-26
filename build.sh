#! /bin/bash

################################################################################
#
# Build Android for the i.MX5 or i.MX6 family
#
# Author: Tom Wang
#
# - Images will be output to the current directory.
# - Customizable parameters are at the top of this file.
#
# USAGE:
# - ${1} BUILD_CHOICE select building image. " EX: uboot, kernel, android or all, package
# - ${2} RTXVER is option if not define will be used default.
# - ${3} TARGET_BOARD select board if not define will be used default.
# RTXVER : 無設定 or - is Default
# RTXVER : menuconfig is for uboot and kernel
# RTXVER : distclean is for kernel


# example:
#    $./build_***.sh all   => build uboot, kernel and android OS
#    $./build_***.sh uboot   => build uboot
#    $./build_***.sh kernel   => build kernel
#    $./build_***.sh android   => build android OS
#    $./build_***.sh package   => create NFS filesystem
#
################################################################################
TOP=`pwd`
ARMCOMMAND="arm-linux-androideabi-"

export LC_ALL=C

#### Target Board ######################################################
#TARGET_BOARD="a6"
#TARGET_BOARD="a6plus"
TARGET_BOARD="pitx"

#######################################
# Custom parameters
#######################################

case "$TARGET_BOARD" in
	"a6")
		DEFAULT_RTXVER="R0031.00"
		;;
	"a6plus")
		DEFAULT_RTXVER="R0051.00"
		;;
	"pitx")
		DEFAULT_RTXVER="R0101.00"
		;;

	*) 
		echo "Please set the target board a6/a6plus/pitx."
        exit 1
		;;
esac

java_version='1.7.0'

uboot_config=0;
kernel_config=0;

BUILD_CHOICE=${1}

if [ -z ${1} ]; then
	echo "ERROR: Need to specify build, e.g.:"
	echo "${0} ${1} all"
	exit 0
fi

RTXVER=${2}
if [ -z ${2} ]; then
	RTXVER=${DEFAULT_RTXVER}
else
    if [ ${2} == '-' ]; then
        RTXVER=${DEFAULT_RTXVER}
    fi
fi

if [ ! -z ${3} ]; then
	TARGET_BOARD=${3}
fi


# Number of CPUs used to build [U-Boot+kernel] and [Android]
# Android can have issues building using several CPUs
CPUS=`cat /proc/cpuinfo | grep processor | wc -l`
CPUS_ANDROID=`cat /proc/cpuinfo | grep processor | wc -l`
#CPUS_ANDROID=1


#######################################
# What to build (set to 0 to disable and > 0 to enable)
#######################################
case "$BUILD_CHOICE" in
	"all")
		BUILD_UBOOT=1
		BUILD_KERNEL=1
		BUILD_ANDROID=1
		BUILD_PACKAGE=0
		;;

	"uboot")
		BUILD_UBOOT=1
		BUILD_KERNEL=0
		BUILD_ANDROID=0
		BUILD_PACKAGE=0
		case "$RTXVER" in
			"menuconfig")
				uboot_config=1;
				;;
		esac
		;;

	"kernel")
		BUILD_UBOOT=0
		BUILD_KERNEL=1
		BUILD_ANDROID=0
		BUILD_PACKAGE=0
		case "$RTXVER" in
			"menuconfig")
				kernel_config=1;
				;;
			"distclean")
				kernel_config=2;
				;;
		esac
		;;

	"android")
		BUILD_UBOOT=0
		BUILD_KERNEL=0
		BUILD_ANDROID=1
		BUILD_PACKAGE=0
		;;
				
	"package")
		BUILD_UBOOT=0
		BUILD_KERNEL=0
		BUILD_ANDROID=0
		BUILD_PACKAGE=1
		;;
	# Add other boards here

	*) 
		echo "Error must define BUILD_CHOICE, e.g.:"
		echo "uboot, kernel, android or all."
		exit 1
		;;
esac


#######################################
# Board-specific parameters
#######################################
ANDROID_OUT_SUFFIX=${TARGET_BOARD}_imx6dq
ANDROID_BUILD_TYPE=eng
case "${TARGET_BOARD}" in
	"a6" )
		UBOOT_CFG=rtx_a6_mx6q_micro1g_dtb_rtx_all_android_defconfig
        TARGET_VENDER="rtx"
		TARGET_SOC="imx6q"
		TARGET_SUBBOARD=""
		;;
	"a6plus" )
		UBOOT_CFG=rtx_a6plus_mx6q_micro1g_dtb_rtx_box_android_defconfig
        TARGET_VENDER="rtx"
		TARGET_SOC="imx6q"
		TARGET_SUBBOARD=""
		;;
	"pitx" )
		UBOOT_CFG=rtx_pitx_mx6q_nanya1g_dtb_rtx_all_android_defconfig
		UBOOT_CFG2G=rtx_pitx_mx6q_nanya2g_dtb_rtx_all_android_defconfig
        TARGET_VENDER="rtx"
		TARGET_SOC="imx6q"
		TARGET_SUBBOARD="b21"
		;;
	
	*)
		echo "Please set the target board a6/a6plus/pitx."
		exit 1
		;;
esac

if [ ! "${TARGET_VENDER}" == "" ] ; then
	KERNEL_PROJECT=${TARGET_VENDER}
else
    echo "Please set the TARGET_VENDER."
    exit 1
fi

if [ ! "${TARGET_SOC}" == "" ] ; then
	KERNEL_PROJECT=${KERNEL_PROJECT}-${TARGET_SOC}
else
    echo "Please set the TARGET_SOC."
    exit 1
fi

if [ ! "${TARGET_BOARD}" == "" ] ; then
	KERNEL_PROJECT=${KERNEL_PROJECT}-${TARGET_BOARD}
else
    echo "Please set the TARGET_BOARD."
    exit 1
fi

if [ ! "${TARGET_SUBBOARD}" == "" ] ; then
	KERNEL_PROJECT=${KERNEL_PROJECT}-${TARGET_SUBBOARD}
fi

KERNEL_PROJECT=${KERNEL_PROJECT}-android

KERNEL_DTB_DTS=${KERNEL_PROJECT}.dts
KERNEL_DTB_DTSI=${KERNEL_PROJECT}-iomux.dtsi
KERNEL_SOC_DTSI=${TARGET_VENDER}-${TARGET_SOC}-soc.dtsi

KERNEL_DTB=${KERNEL_PROJECT}.dtb
KERNEL_PROJECT_CONFIG=${KERNEL_PROJECT}_defconfig

export BOARD_RTX_VENDOR=${TARGET_BOARD}
export UBOOT_CFG=${UBOOT_CFG}
export KERNEL_CFG=${KERNEL_PROJECT_CONFIG}
export KERNEL_DTB=${KERNEL_DTB}

#######################################
# function define
#######################################
function configure_kernel()
{
    if [ ${kernel_config} -ne 2 ] ; then
        cd ${KERNEL_SRC_ROOT}/arch/arm/boot/dts/
        if [ ! -f "${KERNEL_DTB_DTS}" ] ; then
            ln -s ../../../../rtx/dts/${KERNEL_DTB_DTS} ${KERNEL_DTB_DTS}
        fi

        if [ ! -f "${KERNEL_DTB_DTSI}" ] ; then
            ln -s ../../../../rtx/dts/${KERNEL_DTB_DTSI} ${KERNEL_DTB_DTSI}
        fi

        if [ ! -f "${KERNEL_SOC_DTSI}" ] ; then
            ln -s ../../../../rtx/dts/${KERNEL_SOC_DTSI} ${KERNEL_SOC_DTSI}
        fi
        cd -
        
        if [ ! -f .config ]; then
            if [ -f "rtx/configs/${KERNEL_CFG}" ] ; then
                echo "Use the default configure file rtx/configs/${KERNEL_CFG}"
                cd arch/arm/configs
                ln -s ../../../rtx/configs/${KERNEL_CFG} ${KERNEL_CFG}
                cd -
                
                make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} ${KERNEL_CFG}
            else
                echo "The file (${KERNEL_CFG}) is not exist!"
                exit 1
            fi
        else
            echo ".config already exists - not configuring again"
            #make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} oldconfig
        fi
    fi
	
}
#######################################
# Set environment
#######################################

# Android source root directory
ANDROID_SRC_ROOT=$(pwd)

UBOOT_SRC_ROOT=${ANDROID_SRC_ROOT}/bootable/bootloader/uboot-imx
KERNEL_SRC_ROOT=${ANDROID_SRC_ROOT}/kernel_imx
ANDROID_OUT_PATH=${ANDROID_SRC_ROOT}/out/target/product/${ANDROID_OUT_SUFFIX}

# Toolchain for U-Boot
TOOLCHAIN_UBOOT_PATH=${ANDROID_SRC_ROOT}/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin
# Toolchain for kernel
TOOLCHAIN_KERNEL_PATH=${ANDROID_SRC_ROOT}/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/bin

# Save original PATH
PATH_BASE=${PATH}

# Output directory (will contain images)
if [ ! -d ${ANDROID_SRC_ROOT}/out/${TARGET_BOARD} ]; then
    mkdir -p ${ANDROID_SRC_ROOT}/out/${TARGET_BOARD}
fi
OUTPUT_DIR=${ANDROID_SRC_ROOT}/out/${TARGET_BOARD}

#check retronix board fold
retronix_fold_repo=${ANDROID_SRC_ROOT}/repo
BRAN=RTX_NXP_Android601
MANIFEST=RTX_NXP_Android601.xml
if [ ! -f ${retronix_fold_repo} ] ; then
    #~ curl http://commondatastorage.googleapis.com/git-repo-downloads/repo > repo
    cp build/repo repo
    sudo chmod a+x ./repo
    ./repo init -u ssh://git@github.com/RetronixTechInc/android-manifests.git -b ${BRAN} -m ${MANIFEST}
    ./repo sync
    cp build/core/root.mk Makefile   
fi

#######################################
# Export KERNEL and ANDROID Version
#######################################

if [ -z ${RTXVER} ]; then
	echo "Don't assign version.Used internal version number."
else
	export KERNEL=${RTXVER}
	echo "assign kernel version is" ${KERNEL}
	export BUILD_ID=${RTXVER}
	if [ -f ${ANDROID_SRC_ROOT}/build/core/build_id.mk ]; then
		touch ${ANDROID_SRC_ROOT}/build/core/build_id.mk
	fi
	if [ -f ${ANDROID_SRC_ROOT}/device/fsl/build_id.mk ]; then
		touch ${ANDROID_SRC_ROOT}/device/fsl/build_id.mk
	fi
	echo "assign ANDROID version is" ${BUILD_ID}
fi

export BUILD_NUMBER=$(date +"%Y%m%d")
echo "assign BUILD_NUMBER is" ${BUILD_NUMBER}
#######################################
# Display settings
#######################################

echo "################################################"
echo "Building:"
echo "  TARGET_BOARD=${TARGET_BOARD}"
echo "  BUILD_UBOOT=${BUILD_UBOOT}"
echo "  BUILD_KERNEL=${BUILD_KERNEL}"
echo "  BUILD_ANDROID=${BUILD_ANDROID}"
echo "Using:"
echo "  CPUS=${CPUS}"
echo "  CPUS_ANDROID=${CPUS_ANDROID}"
echo "################################################"


#######################################
# Build U-Boot
#######################################
if [ ${BUILD_UBOOT} -gt 0 ]; then

	cd ${UBOOT_SRC_ROOT}	|| exit 1

    if [ ${uboot_config} -gt 0 ] ; then
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND} menuconfig
		
	else
		if [ ! -z ${UBOOT_CFG2G} ]; then
			make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND} distclean
			make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND} ${UBOOT_CFG2G}
			make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND}  -j${CPUS} && \
			cp u-boot.imx ${OUTPUT_DIR}/u-boot-2g.imx
		fi
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND} distclean
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND} ${UBOOT_CFG}
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_UBOOT_PATH}/${ARMCOMMAND}  -j${CPUS} && \
		cp u-boot.imx ${OUTPUT_DIR}/u-boot.imx
	fi
	
	cd - || exit 1
	
fi


#######################################
# Build the kernel
#######################################
#mkimage -A arm -O linux -T kernel -C none -a 0x80008000 -e 0x80008000 -n "Android Linux Kernel" -d ./zImage ./uImage
if [ ${BUILD_KERNEL} -gt 0 ]; then
	cd ${KERNEL_SRC_ROOT}
	
	# Only configure if there is no .config
	configure_kernel
	if [ ${kernel_config} -eq 1 ] ; then
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} menuconfig
	elif [ ${kernel_config} -eq 2 ] ; then
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} distclean
		exit 0
	else
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} uImage LOADADDR=0x10008000 KCFLAGS=-mno-android -j${CPUS} && \
		cp arch/arm/boot/uImage ${OUTPUT_DIR}
		make ARCH=arm CROSS_COMPILE=${TOOLCHAIN_KERNEL_PATH}/${ARMCOMMAND} ${KERNEL_DTB} && \
		cp arch/arm/boot/dts/${KERNEL_DTB} ${OUTPUT_DIR}
	fi
	cd - || exit 1
	
fi


#######################################
# Build Android
#######################################
# Build
if [ ${BUILD_ANDROID} -gt 0 ]; then
cd ${ANDROID_SRC_ROOT}	|| exit 1
	export PATH=${UBOOT_SRC_ROOT}/tools:${PATH}

	#build.prop
	# adb over ethernet "5555" => ready export ADBHOST=dev-ip
	export PROJECT_ADB_PORT="5555"

	# system language => ready
	# en_US en_IN fr_FR it_IT es_ES et_EE de_DE nl_NL cs_CZ pl_PL ja_JP zh_TW zh_CN zh_HK ru_RU ko_KR nb_NO es_US da_DK el_GR tr_TR pt_PT pt_BR rm_CH sv_SE bg_BG ca_ES en_GB fi_FI hi_IN hr_HR hu_HU in_ID iw_IL lt_LT lv_LV ro_RO sk_SK sl_SI sr_RS uk_UA vi_VN tl_PH ar_EG fa_IR th_TH sw_TZ ms_MY af_ZA zu_ZA am_ET hi_IN en_XA ar_XB fr_CA km_KH lo_LA ne_NP si_LK mn_MN hy_AM az_AZ ka_GE
	export PROJECT_LANGUAGE="en"
	export PROJECT_COUNTRY="US"
	export PROJECT_LOCALEVAR=""
	
	# timezone => ready
	export PROJECT_TIMEZONE="Asia/Taipei"
	# time sync service web => ready
	export PROJECT_NTP_SERVER="time.stdtime.gov.tw"
	#export PROJECT_NTP_SERVER="clock.stdtime.gov.tw"

	# su root mode 1:enable 0:disable => ready
	export PROJECT_SUROOT_MODE="1"
	
	# fixed audio output port at analog 1:enable 0:disable => ready
	export PROJECT_FIXED_AUDIO="0"

    # navigationbar 1:enable 0:disable => ready
    export PROJECT_HIDE_NAVIGATIONBAR="0"
        
    # Disable Eng Red Box => ready
	export PROJECT_DISABLE_REDBOX="1"

	source ${ANDROID_SRC_ROOT}/build/envsetup.sh && \
	lunch ${ANDROID_OUT_SUFFIX}-${ANDROID_BUILD_TYPE} && \
	make -j${CPUS_ANDROID} 2>&1 | tee build_${ANDROID_OUT_SUFFIX}_android.log

	# copy images
	if [ $? -eq 0 ]; then
		BUILD_PACKAGE=1
	fi
	
fi

#######################################
# Build Android
#######################################
# Build
if [ ${BUILD_PACKAGE} -gt 0 ]; then
	cd ${ANDROID_OUT_PATH} || exit 1
	
	sudo rm ${OUTPUT_DIR}/system.img
	sync
	
	cp ramdisk.img ${OUTPUT_DIR}/ramdisk.img.gz	&& \
	simg2img system.img system_raw.img || exit 1
	sync
    cp system_raw.img ${OUTPUT_DIR}/system.img
    sync
	
    #cp ramdisk.img ${OUTPUT_DIR}/ramdisk.img.gz	&& \
	#cp system.img ${OUTPUT_DIR}/system.img || exit 1
	#sync
	
	
	cd ${OUTPUT_DIR} || exit 1
	sudo rm -rf ${OUTPUT_DIR}/ramdisk
	
	gunzip ramdisk.img.gz; mkdir ramdisk; cd ramdisk; cpio -i --no-absolute-filenames < ../ramdisk.img && \
	sudo mount ../system.img system
	
	#Create filesystem
		#mask /dev/block/mmcblk0p5 mount at /system
	#sed -i "s/\/dev\/block\/mmcblk0p5/#\/dev\/block\/mmcblk0p5/g" ${OUTPUT_DIR}/ramdisk/fstab.retronix
		#mask root remount as read-only
	#sed -i "s/mount\ rootfs\ rootfs\ \/\ ro\ remount/#mount\ rootfs\ rootfs\ \/\ ro\ remount/g" ${OUTPUT_DIR}/ramdisk/init.rc
		#for ADB over Ethernet
	#sed -i "/\ \ \ \ setprop\ ro.adb.secure/a\ \ \ \ setprop\ service.adb.tcp.port\ 5555" ${OUTPUT_DIR}/ramdisk/init.rc
	#sed -i "s/\ \ \ \ setprop\ ro.adb.secure/#\ \ \ \ setprop\ ro.adb.secure/g" ${OUTPUT_DIR}/ramdisk/init.rc
	sudo tar -cpvf ${OUTPUT_DIR}/android-${RTXVER}-fs.tar ./* && \
	sync

	sudo umount system && \
	cd ${OUTPUT_DIR} && \
	sudo rm -rf ${OUTPUT_DIR}/ramdisk/*
	sudo rm ${OUTPUT_DIR}/ramdisk.img && \
	
	sudo tar --numeric-owner -xpvf ${OUTPUT_DIR}/android-${RTXVER}-fs.tar -C ramdisk/

	

fi
