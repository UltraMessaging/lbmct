#!/bin/sh
# tst.sh

D=`pwd`
cd ..
. ./lbm.sh
cd "$D"

JAVA_ROOT=`pwd`

cd $JAVA_ROOT
java -cp .:$JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtRcv &

java -cp .:$JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtSrc

cd $JAVA_ROOT/test/com/latencybusters/lbmct
java -cp $JAVA_ROOT/main/lbmct.jar:$JAVA_ROOT/main/lbmext.jar:$LBM_JAR:$JUNIT_JAR:$HAMCREST_JAR:$JAVA_ROOT/test/test.jar -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" org.junit.runner.JUnitCore com.latencybusters.lbmct.LbmCtTest
