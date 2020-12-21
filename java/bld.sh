#!/bin/sh
# bld.sh

D=`pwd`
cd ..
. ./lbm.sh
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
