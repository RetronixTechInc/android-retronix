# This script is used to fetch the complete source for android build
# It will install android NXP release in WORKSPACE dir (current path by default)

echo "Start fetching the source for android build"

if [ "x$BASH_VERSION" = "x" ]; then
    echo "ERROR: script is designed to be sourced in a bash shell." >&2
    return 1
fi

# retrieve path of release package
REL_PACKAGE_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
echo "Setting REL_PACKAGE_DIR to $REL_PACKAGE_DIR"

if [ -z "$WORKSPACE" ];then
    WORKSPACE=$PWD
    echo "Setting WORKSPACE to $WORKSPACE"
fi

if [ -z "$android_builddir" ];then
    android_builddir=$WORKSPACE/android601
    echo "Setting android_builddir to $android_builddir"
fi

if [ ! -d "$android_builddir" ]; then
    # Create android build dir if it does not exist.
    mkdir "$android_builddir"
    cd "$android_builddir"
    cp -rf "$REL_PACKAGE_DIR/build/repo" "$android_builddir/"
    repo init -u https://github.com/RetronixTechInc/android-manifests.git -b RTX_NXP_Android601 -m RTX_NXP_Android601.xml
      rc=$?
      if [ "$rc" != 0 ]; then
         echo "---------------------------------------------------"
         echo "-----Repo Init failure"
         echo "---------------------------------------------------"
         return 1
      fi
else
    cd "$android_builddir"
fi

repo sync
rc=$?
if [ "$rc" != 0 ]; then
 echo "---------------------------------------------------"
 echo "------Repo sync failure"
 echo "---------------------------------------------------"
 return 1
fi

#~ # Copy all the proprietary packages to the android build folder
cp -rf "$REL_PACKAGE_DIR/build.sh" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/build/core/root.mk" "$android_builddir/Makefile"
cp -rf "$REL_PACKAGE_DIR/art" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/bionic" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/bootable" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/build" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/device" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/external" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/frameworks" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/hardware" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/libcore" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/packages" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/prebuilts" "$android_builddir/"
cp -rf "$REL_PACKAGE_DIR/system" "$android_builddir/"
# unset variables

#~ cd $WORKSPACE

unset android_builddir
unset WORKSPACE
unset REL_PACKAGE_DIR

echo "Android source is ready for the build"
