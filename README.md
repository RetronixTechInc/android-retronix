<h1>android-retronix</h1>  
Create Retronix PICO Board android6.0.1 BSP.
<h2>Download source code </h2>  
<p>
$ create fold : <br>
 mkdir android601 <br>
$ enter to fold   :  <br>
 cd android601 <br>
$ download repo   :  <br>
 curl https://dl-ssl.google.com/dl/googlesource/git-repo/repo > repo <br>
$ chmod repo      :  <br>
 sudo chmod 777 repo <br>
$ modify repo add ssl :  <br>
 import ssl <br>
 ssl._create_default_https_context = ssl._create_unverified_context <br>
$ download source :  <br>
 ./repo init -u git@github.com:RetronixTechInc/android-manifests -b RTX_NXP_Android601 -m RTX_NXP_Android601.xml <br>
$ sync source code:  <br>
 ./repo sync <br>
</p>  

<h2>Build source code </h2>  
<p>  
$ ./build.sh all <br>
The BSP images at out/device fold if success. 
</p>  

<h2>Create SD to write device from build image</h2>  
Prepare micro SD card the value greater than 4GB.  

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
Prepare micro SD card the value greater than 4GB.  

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
