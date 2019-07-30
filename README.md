<h1>android-retronix</h1>  
Create Retronix PICO Board android6.0.1 BSP.

<h2>clone project then excute "build.sh all"</h2>  
<p>
$ clone https://github.com/RetronixTechInc/android-retronix -b RTX_NXP_Android601  
$ ./build.sh all
</p>  
It will be download repo, init, sync and build uboot,kernel,android BSP.  
The BSP images at out/device fold if success. 

<h2>Create SD to write device</h2>  
Prepare micro SD card the volue greater than 4GB.  

Copy ***.tar, uImage, ***.dtb, ***.imx from out//device fold to device/rtx/tools/pitx-android601/flash/ fold. 

Insert micro SD card and check node.  
 
$ls /dev/sd* //assume is /dev/sde  

$cd device/rtx/tools/  

$./make-mksd.sh pitx-android601 /dev/sdc  

Switch device switch bootup from external sd card.

Inser micro SD card to your device(PICO board).  

Power on your device, you can see the write success at UI and debug port.  

Plug out micro SD card than restart yout device.  

You can see android UI when it start up finished.  

