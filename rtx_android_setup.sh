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
    android_builddir=$WORKSPACE/android_build
    echo "Setting android_builddir to $android_builddir"
fi

if [ ! -d "$android_builddir" ]; then
    # Create android build dir if it does not exist.
    mkdir "$android_builddir"
    cd "$android_builddir"
    repo init -u git@github.com:RetronixTechInc/android-manifests -b RTX_PICO_Android900 -m RTX_PICO_Android900.xml
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

# Copy all the proprietary packages to the android build folder

cp -r "$REL_PACKAGE_DIR"/EULA.txt "$android_builddir"
cp -r "$REL_PACKAGE_DIR"/SCR* "$android_builddir"
cp -rf "$REL_PACKAGE_DIR"/rtx/sabresd_6dq/* "$android_builddir"/device/fsl/imx6dq/sabresd_6dq/
cp -r "$REL_PACKAGE_DIR"/rtx/tools/* "$android_builddir"/device/fsl/common/tools/
cp -r "$REL_PACKAGE_DIR"/rtx/partition/* "$android_builddir"/device/fsl/common/partition/
cp -rf "$REL_PACKAGE_DIR"/frameworks/* "$android_builddir"/frameworks/
cp -rf "$REL_PACKAGE_DIR"/vendor/* "$android_builddir"/vendor/
# unset variables

unset android_builddir
unset WORKSPACE
unset REL_PACKAGE_DIR

echo "Android source is ready for the build"
