#!/bin/bash
####################################
# make-mksd.sh input_fold dev_node
#  sudo ./make-mksd.sh a6/a6plus/pitx /dev/sdc
#  sudo ./make-mksd.sh a6/a6plus/pitx /dev/sdc

####################################
#partition 0 is 64MB
MBR_SIZE=64
#dev_node size small than 64GB
DEV_LIMIT=64

DIR=$0
CURDIR=${DIR%/*}
MYPATHSD=`pwd`
version_date=`date +'%Y%m%d'`

####################################
# help function
####################################
help() {
bn=`basename $0`
cat << EOF
usage $bn fold_name device_node
    make mksd-card and mksd.zip
option:
  -h    displays this help message
  -m	create mksd-********.zip for release
  -c	create null update sdcard
example:
sudo ./make-mksd.sh input_fold /dev/sdc
sudo ./make-mksd.sh -m input_fold /dev/sdc

EOF

}

print() {
#	echo $1 > ${MESSAGE_PORT}
	echo $1 
}

run_end() {
	cd ${MYPATH}
	print "Create Fail!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
	exit 0
}
####################################
# argc and argv check
####################################
sd_only=1
sd_null=0
sd_release=0

if [ $# -lt 1 ]; then
    help; exit ;
fi

case ${1} in
    -h) help; exit ;;
    -m) sd_only=0 ; shift ;;
    -c) sd_null=1 ; shift ;;
    -r) sd_release=1 ; shift ;;
    *)   ;;
esac

if [ ${sd_release} -eq 1 ]; then
    sd_null=0
fi

#echo "Tom2 ===== $#"
if [ $# -lt 2 ]; then
    help; exit ;
fi

#get input fold
if [ -z ${1} ]; then
    echo "need input file."
    exit 0
else
    if [ -d ${1} ]; then
        in_fold=${1}
    else
        echo "can't find fold ${1}"
        exit 0
    fi
fi

#get device node
if [ -z ${2} ]; then
    echo "/dev/sdc or /dev/mmcblk0 or /dev/loop0"
    exit 0
else
    if [ -b ${2} ]; then
        DEVNODE=${2}
    else
        echo "can't find node ${2}"
        exit 0
    fi
fi

#get part variable is p if mmcblk device.
part=""
echo ${DEVNODE} | grep mmcblk > /dev/null
if [ "$?" -eq "0" ]; then
	part="p"
fi

# check the if root?
userid=`id -u`
if [ $userid -ne "0" ]; then
	echo "you're not root?"
	exit
fi

#get files path.
SYSPATH="${CURDIR}/${in_fold}"
DATAPATH="${CURDIR}/${in_fold}/flash"

ENGKERNEL=`ls ${SYSPATH}/uImage-*`
ENGDTB=`ls ${SYSPATH}/*.dtb`
ENGRAMDISK=`ls ${SYSPATH}/uramdisk-*.img`

UBOOTVAR=`ls ${SYSPATH}/u-boot.arg`
SETTINGVAR=`ls ${SYSPATH}/setting*.bin`
    
UBOOT=`ls ${DATAPATH}/u-boot*.imx`
KERNEL=`ls ${DATAPATH}/uImage*`
DTB=`ls ${DATAPATH}/*.dtb`

if [ "$UBOOT"x = "x" ] ;    then run_end ; fi
if [ "$KERNEL"x = "x" ] ;   then run_end ; fi
if [ "$ENGKERNEL"x = "x" ] ;   then run_end ; fi

####################################
# calculate size function
####################################
calculate_fold_size()
{
    size=`du -c -BM ${in_fold} | grep total | awk '{print $1}'`
    CREATE_SIZE=`expr ${size%%M} + 128 `
}


calculate_size()
{
    BIN_SIZE=`expr ${MBR_SIZE} +  ${CREATE_SIZE} + 32`
    MBR_SIZE=`expr $MBR_SIZE \* 1024 \* 1024`
    SYSTEM_SIZE=`expr $CREATE_SIZE \* 1024 \* 1024`
}

check_dev_size()
{
    DEV_SIZE=`fdisk -l ${DEVNODE} | grep "Disk ${DEVNODE}:" | awk '{print $5}'`
    UNITS_SIZE=`fdisk -l ${DEVNODE} | grep 'Units = cylinders' | awk '{print $9}'`

    if [ -z ${UNITS_SIZE} ]; then
    UNITS_SIZE=`fdisk -l ${DEVNODE} | grep 'Units = sectors' | awk '{print $9}'`
    fi

    if [ -z ${UNITS_SIZE} ]; then
    UNITS_SIZE=`fdisk -l ${DEVNODE} | grep 'Units: sectors' | awk '{print $8}'`
    fi

    if [ -z ${UNITS_SIZE} ]; then
    UNITS_SIZE="512"
    fi

    DEV_LIMIT_SIZE=`expr ${DEV_LIMIT} \* 1024 \* 1024 \* 1024`
    if [ ${DEV_SIZE} -gt ${DEV_LIMIT_SIZE} ] ; then
        echo "Device size is biger than ${DEV_LIMIT} GB "
        exit
    fi
}

####################################
# umount function
####################################
umount_sd()
{
	mounted=$(mount | grep $DEVNODE | awk '{print $1}' | wc -l)
    if [ $mounted -gt 0 ]; then
		todo=$(mount | grep $DEVNODE | awk '{print $1}')
        for i in $todo
			do
                echo $i
				sudo umount $i
                sleep 1
			done
	fi
}

mount_loop()
{
    if [ -f loop.img ]; then
        sudo rm -rf loop.img
    fi
    sudo dd if=/dev/zero of=loop.img bs=1M count=${BIN_SIZE}
    
    sudo losetup ${DEVNODE} loop.img
}

####################################
# create_dev_partition function
####################################
create_dev_partition()
{
    echo "fdisk ${DEVNODE}"
    rm -f ${MYPATHSD}/.SD_PARTITION
    touch ${MYPATHSD}/.SD_PARTITION
    # - P1 ----------------------------------
    MBR_SIZE=`expr $MBR_SIZE / $UNITS_SIZE`
    SYSTEM_SIZE=`expr $MBR_SIZE + $SYSTEM_SIZE / $UNITS_SIZE`

    echo "n" >> ${MYPATHSD}/.SD_PARTITION
    echo "p" >> ${MYPATHSD}/.SD_PARTITION
    echo "1" >> ${MYPATHSD}/.SD_PARTITION
    echo "$MBR_SIZE" >> ${MYPATHSD}/.SD_PARTITION
    if [ ${sd_null} -eq 0 ] ;  then
        echo "$SYSTEM_SIZE" >> ${MYPATHSD}/.SD_PARTITION
    else
        echo "" >> ${MYPATHSD}/.SD_PARTITION
    fi
    echo "" >> ${MYPATHSD}/.SD_PARTITION

    # - Finish write-------------------------
    echo "w" >> ${MYPATHSD}/.SD_PARTITION
    echo "" >> /.SD_PARTITION

    sudo fdisk ${DEVNODE} < ${MYPATHSD}/.SD_PARTITION
    sync
    sleep 5
    rm ${MYPATHSD}/.SD_PARTITION
}

####################################
# write_dev_p0 function
####################################
write_dev_p0()
{
    echo "clean 0M~60M"
    # clean 60M
    sudo dd if=/dev/zero of=${DEVNODE} bs=1M count=60

    create_dev_partition

    echo "format ${DEVNODE}${part}?"
    sudo mkfs.fat ${DEVNODE}${part}1

    # u-boot image at 1KB
    if [ "$UBOOT"x != "x" ] ;  then
        sudo dd if=${UBOOT} of=${DEVNODE} bs=1024 seek=1
        sync
    fi

    # kernel image at 1MB and 13MB
    if [ "${ENGKERNEL}"x != "x" ] ;  then
        sudo dd if=${ENGKERNEL} of=${DEVNODE} bs=1024 seek=1024 || run_end
        sync
        sudo dd if=${ENGKERNEL} of=${DEVNODE} bs=1024 seek=13312 || run_end
        sync
    fi
    
    # dtb at 11.5MB and 12.5MB
    if [ "${ENGDTB}"x != "x" ] ;  then
        sudo dd if=${ENGDTB} of=${DEVNODE} bs=1024 seek=11776 || run_end
        sync
        sudo dd if=${ENGDTB} of=${DEVNODE} bs=1024 seek=12800 || run_end
        sync
    fi
    
    # engramdisk image at 30MB
    if [ "${ENGRAMDISK}"x != "x" ] ;  then
        sudo dd if=${ENGRAMDISK} of=${DEVNODE} bs=1M seek=30 || run_end
        sync
    fi
    
    # setting parameter at 768KB
    if [ "$SETTINGVAR"x != "x" ] ;  then
        sudo dd if=${SETTINGVAR} of=${DEVNODE} bs=1024 seek=768
        sync
    fi
    
    # u-boot parameter at 12MB
    if [ "$UBOOTVAR"x != "x" ] ;  then
        sudo dd if=${UBOOTVAR} of=${DEVNODE} bs=1024 seek=12288
        sync
    fi
    
}

####################################
# copy_to_flash_fold function
####################################
copy_to_flash_fold()
{
    cp -rf ${CURDIR}/${in_fold}/* ${CURDIR}/mnt/
    sync
    
    sync
    sudo umount ${CURDIR}/mnt
    sync
    sudo rm -rf ${CURDIR}/mnt
    sync
}

####################################
# run_dev function
####################################
run_dev() {
    #umount $DEVNODE
    umount_sd

    #calculate size
    calculate_fold_size
    calculate_size

    #get UNITS_SIZE
    check_dev_size

    write_dev_p0

    if [ ${sd_null} -eq 0 ] ;  then
        if [ ! -d ${CURDIR}/mnt ]; then
            mkdir -p ${CURDIR}/mnt
        fi
        sudo rm -rf ${CURDIR}/mnt/*
        sync
        
        sudo mount ${DEVNODE}${part}1 ${CURDIR}/mnt
        
        copy_to_flash_fold

        if [ ${sd_only} -eq 0 ] ;  then
            cd ${CURDIR}/mksd
            sudo dd if=${DEVNODE} of=./bindata.bin bs=1M count=${BIN_SIZE}
            sync

            md5sum ./bindata.bin > file.lst
            cd ..
            sync

            zip -r ${CURDIR}/mksd-${in_fold}-${version_date}.zip ${CURDIR}/mksd/*
        fi
    fi
}

####################################
# run_loop function
####################################
run_loop() {

    #calculate size
    calculate_fold_size
    calculate_size

    #create $DEVNODE
    mount_loop

    #get UNITS_SIZE
    check_dev_size

    write_dev_p0

    #formate partition
    sudo losetup -d ${DEVNODE}
    offset_size=`expr $MBR_SIZE \* $UNITS_SIZE`

    echo "format ${DEVNODE}"
    sudo losetup -o ${offset_size} ${DEVNODE} loop.img
    sudo mkfs.fat ${DEVNODE}

    if [ ! -d ${CURDIR}/mnt ]; then
        mkdir -p ${CURDIR}/mnt
    fi
    sudo rm -rf ${CURDIR}/mnt/*
    sync

    sudo mount ${DEVNODE} ${CURDIR}/mnt
    
    copy_to_flash_fold
    
    sudo losetup -d ${DEVNODE}
    sync

    mv loop.img ${CURDIR}/mksd/bindata.bin
    sync

    cd ${CURDIR}/mksd
    md5sum ./bindata.bin > file.lst
    cd ..
    sync

    zip -r ${CURDIR}/mksd-${in_fold}-${version_date}.zip ${CURDIR}/mksd/*

} 

if [ ${sd_release} -eq 1 ] ;  then
    run_loop
else
    run_dev
fi

sync

echo "mksd success!!!"

