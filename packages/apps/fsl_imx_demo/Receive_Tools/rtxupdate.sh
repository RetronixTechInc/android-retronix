#!/system/bin/sh
MYPATH="/storage/"
DESPATH="/data/local/update.sh"

updatesc="update.sh"
updatecheckcode="usbcheckcode"

comfirmstr="update system from usb storage!"
comfirmread="false"


ls_storage_list=`ls ${MYPATH}`

for storage_fold in ${ls_storage_list}; do
	if [ ${storage_fold} != "emulated" -a ${storage_fold} != "self" ]; then
        SEARCHPATH="${MYPATH}${storage_fold}"
        updatesc=`ls ${SEARCHPATH}/${updatesc}`
        updatecode=`ls ${SEARCHPATH}/${updatecheckcode}`

        if [ "$updatecode"x != "x" ] ;  then
            comfirmread=`cat ${updatecode}`
            if [ "$comfirmread" == "$comfirmstr" ] ;  then
                if [ "$updatesc"x != "x" ] ;  then
                    cp $updatesc $DESPATH
                    $DESPATH
                    rm $DESPATH
                fi
            fi
        fi
	fi
done



