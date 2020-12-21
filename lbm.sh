# lbm.sh
# 1. Replace the RELPATH and PLATFORM lines with your own values.
# 2. Create lbm.lic with your license information.

RELPATH=$HOME/UMP_6.12.1
LBM_JAR=$RELPATH/UMS_6.12.1.jar
#PLATFORM=Linux-glibc-2.17-x86_64
PLATFORM=Darwin-10.8.0-x86_64

JUNIT_JAR=$HOME/lib/junit-4.13-beta-3.jar
HAMCREST_JAR=$HOME/lib/hamcrest-core-1.3.jar
#GTEST=$HOME/lib/gtest/Linux-glibc-2.12-x86_64/googletest
GTEST=$HOME/lib/googletest
export LBM_JAR JUNIT_JAR HAMCREST_JAR GTEST

# UM license key file: lbm.lic
if [ -f lbm.lic ];  then  :
  LBM_SH_D=`pwd`
  LBM_LICENSE_FILENAME="$LBM_SH_D/lbm.lic"
  export LBM_LICENSE_FILENAME
else  :
  echo "lbm.sh: 'lbm.lic' not found"  >&2
fi

LBM_BASE=$RELPATH
TARGET_PLATFORM=$PLATFORM
LBM_PLATFORM=$LBM_BASE/$TARGET_PLATFORM
LBM_INCLUDE=$LBM_PLATFORM/include
export LBM_BASE TARGET_PLATFORM LBM_INCLUDE LBM_PLATFORM

LD_LIBRARY_PATH=$LBM_PLATFORM/lib
DYLD_LIBRARY_PATH=$LBM_PLATFORM/lib
PATH="$LBM_PLATFORM/bin:$PATH"
export LD_LIBRARY_PATH DYLD_LIBRARY_PATH PATH
