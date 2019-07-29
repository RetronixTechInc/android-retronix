#!/bin/bash 

echo target_product is [${TARGET_PRODUCT}]
echo ""

sleep 1

if [ "${TARGET_PRODUCT}" = "" ];then
        echo "Error: please run lunch to provide a valid TARGET_PRODUCT"
        echo ""
            exit -1
fi


cd out/target/product/${TARGET_PRODUCT}/

if [ "$?" != 0 ];then
    exit -1
fi

echo "Do clean up in out/target/product/${TARGET_PRODUCT}/..."
echo ""

echo "Press Ctrl+c to break..."
echo ""

sleep 2

rm -rf `find . -name "lib_omx_*"`
rm -rf `find . -name "lib_vpu_wrapper*"`
rm -rf `find . -name "libmediaplayer*"`
rm -rf `find . -name "media_*.xml*"`
rm -rf `find . -name "*register*"`
rm -rf `find . -name "libstagefright*"`
rm -rf `find . -name "CactusPlayer.apk*"`
rm -rf `find . -name "lib*_dec_*"`
rm -rf `find . -name "lib*_enc_*"`
rm -rf `find . -name "lib*_parser_*"`
rm -rf `find . -name "lib*_wrap*"`
rm -rf `find . -name "lib_peq*"`
rm -rf `find . -name "lib_mp4_muxer*"`
rm -rf `find . -name "libfsl_wfd*"`
rm -rf `find . -name "CactusPlayer.odex"`
rm -rf `find . -name "WfdSink.apk"`

echo "finished."

cd -
