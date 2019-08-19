<h1>android-retronix</h1>  
Create Retronix PICO Board android6.0.1 BSP.

<h2>clone project then excute "build.sh all"</h2>  
<p>
$ clone https://github.com/RetronixTechInc/android-retronix -b RTX_NXP_Android601  
$ ./build.sh all
</p>  
It will be download repo, init, sync and build uboot,kernel,android BSP.  
The BSP images at out/device fold if success. 

<h2>Create SD to write device from build image</h2>  
Prepare micro SD card the volue greater than 4GB.  

Copy ***.tar, uImage, ***.dtb, ***.imx from out//device fold to device/rtx/tools/pitx-android601/flash/ fold. 

Insert micro SD card and check node.  
 
$ls /dev/sd* //assume is /dev/sdc  

$cd device/rtx/tools/  

$./make-mksd.sh pitx-android601 /dev/sdc  

Switch device switch bootup from external sd card.

Inser micro SD card to your device(PICO board).  

Power on your device, you can see the write success at UI and debug port.  

Plug out micro SD card and switch device switch bootup from internal emmc than restart yout device.  

You can see android UI when it start up finished.  


<h2>Create SD to write device from Retronix pre-build image</h2>  
Prepare micro SD card the volue greater than 4GB.  

Insert micro SD card and check node.  

$ls /dev/sd* //assume is /dev/sdc  

Download <a href="https://drive.google.com/file/d/1D3M5P9JKwUFZLCeEPRbDYV2SRwAnaoat/view?usp=sharing" style="text-decoration:none;color:red;">mksd</a> zip file.

Extract it and write bindata.bin file to sd card:  
$ sudo dd if=bindata.bin of=/dev/sdc bs=1M

Download <a href="https://drive.google.com/file/d/1jO5p996mNoH78IojQBlLqwD_bsnvWar_/view?usp=sharing" style="text-decoration:none;color:red;">pre-build image</a> and extract it to micro sd card.  


Switch device switch bootup from external sd card.

Inser micro SD card to your device(PICO board).  

Power on your device, you can see the write success at UI and debug port.  

Plug out micro SD card and switch device switch bootup from internal emmc than restart yout device.  

You can see android UI when it start up finished. 
