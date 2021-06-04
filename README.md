<html>
<body>
<h1>android-retronix</h1>
Create Retronix PICO Board android9.0.0 BSP.
<h2>To use this manifest repo, the 'repo' tool must be installed first.</h2>
$: mkdir ~/bin<br>
$: curl https://storage.googleapis.com/git-repo-downloads/repo  > ~/bin/repo<br>
$: chmod a+x ~/bin/repo<br>
$: export PATH=${PATH}:~/bin<br>

<h2>Download and Setup Source:</h2>
$git clone https://github.com/RetronixTechInc/android-retronix -b RTX_PICO_Android900<p>

Run setup script to download Android open source and copy proprietary to source folders<br>
Run from above the folder for setup script to work properly<br>
$: source ./android-retronix/rtx_android_setup.sh<p>

<h2>Setup build environment:</h2>
- Set up the environment for building. This only configures the current terminal.<p>
$ source build/envsetup.sh

<h2>Execute the Android lunch command:</h2>
- Execute the Android lunch command.
lunch command can be issued with an argument or can be issued without the argument presenting a menu of selection.<p>
$ lunch sabresd_6dq-userdebug<p>

Check Android User's Guide for more details<br>

<h2> Execute the make command to generate the image. </h2>
$ make 2>&1 | tee build-log.txt<p>
When the make command is complete, the build-log.txt file contains the execution output.<br>
Check for any errors.<br>

<h2> Download all images to the SD card : </h2>
The minimum size of the SD card is 8 GB.<br>
$: cd out/target/product/sabresd_6dq/<p>
If the SD card is 8 GB, use sudo <br>
$: sudo ./fsl-sdcard-partition.sh -f imx6q /dev/sdx<p>
If the SD card is 16 GB, use sudo <br>
$: sudo ./fsl-sdcard-partition.sh -f imx6q -c 14 /dev/sdx<p>

<h2> Boot from external sdcard </h2>
Switch PICO board's dip sw1 to 0000 (SDCARD).<br>
Plug in micro SD card.<br>
Power on board, you can see android UI.<br>
</body>
</html>

