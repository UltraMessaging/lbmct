#!/bin/sh
# bld.sh

D=`pwd`
cd ..

RELPATH=$HOME/UMP_6.12.1
LBM_JAR=$RELPATH/UMS_6.12.1.jar
#PLATFORM=Linux-glibc-2.17-x86_64
PLATFORM=Darwin-10.8.0-x86_64

JUNIT_JAR=$HOME/lib/junit-4.13-beta-3.jar
HAMCREST_JAR=$HOME/lib/hamcrest-core-1.3.jar
export LBM_JAR JUNIT_JAR HAMCREST_JAR

# Source the file that defines env var for license key.
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
cd "$D"

JAVA_ROOT=`pwd`

rm -f */*.jar

cd $JAVA_ROOT/main/com/latencybusters/lbm
rm -f *.class
javac -cp .:$LBM_JAR *.java
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT/main
jar -cf lbmext.jar com/latencybusters/lbm/*.class
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT/main/com/latencybusters/lbmct
rm -f *.class
javac -cp .:$LBM_JAR:$JAVA_ROOT/main/lbmext.jar:$HAMCREST_JAR:$JUNIT_JAR *.java
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT/main
jar -cf lbmct.jar com/latencybusters/lbmct/*.class
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT/test/com/latencybusters/lbmct
rm -f *.class
javac -cp .:$LBM_JAR:$JAVA_ROOT/main/lbmext.jar:$JAVA_ROOT/main/lbmct.jar:$HAMCREST_JAR:$JUNIT_JAR *.java
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT/test
jar -cf test.jar com/latencybusters/lbmct/*.class
if [ $? -ne 0 ]; then exit 1; fi

cd $JAVA_ROOT
rm -f *.class
javac -cp .:$LBM_JAR:$JAVA_ROOT/main/lbmext.jar:$JAVA_ROOT/main/lbmct.jar *.java


cd $JAVA_ROOT
###java -cp $JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR:$JUNIT_JAR:$HAMCREST_JAR:$JAVA_ROOT/test/test.jar -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" org.junit.runner.JUnitCore com.latencybusters.lbmct.LbmCtTest

java -cp .:$JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtRcv &

java -cp .:$JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtSrc
