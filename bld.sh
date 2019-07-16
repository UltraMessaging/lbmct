#!/bin/sh
# bld.sh


# The following shell vars are specific to your local installation
# conventions.  Change them appropriately.  The JUNIT and HAMCREST
# vars are only needed if you plan to run the selftest.

JAVA_HOME=""
if echo $OSTYPE | egrep -i "linux" >/dev/null; then
  LBM_LICENSE_INFO="`cat $HOME/um.license`"
  LBM_PLATFORM="$HOME/UMQ_6.11.1/Linux-glibc-2.5-x86_64"
  LD_LIBRARY_PATH="$LBM_PLATFORM/lib"
  UM_JARS="$HOME/usr/local/lib"
  JAVA_HOME="/usr/local/jdk-11.0.2/bin"
  JUNIT="$UM_JARS/junit-4.13-beta-3.jar"
  HAMCREST="$UM_JARS/hamcrest-core-1.3.jar"
fi
if echo $OSTYPE | egrep -i "darwin" >/dev/null; then
  LBM_LICENSE_INFO="`cat $HOME/um.license`"
  LBM_PLATFORM="/usr/local"
  # Don't need LD_LIBRARY_PATH or DYLD_LIBRARY_PATH if use /usr/local
  UM_JARS="$LBM_PLATFORM/lib"
  JAVA_HOME="/usr/bin"
  JUNIT="$UM_JARS/junit-4.13-beta-3.jar"
  HAMCREST="$UM_JARS/hamcrest-core-1.3.jar"
fi
if [ "$JAVA_HOME" = "" ]; then echo "Unknown platform" >&2; exit 1; fi
$JAVA_HOME/java -version

# Export the vars that need to be in the environment.
export LBM_LICENSE_INFO
export LD_LIBRARY_PATH

# Build java stuff

rm -rf javadoc

cd java

# The built jar files will go in the java directory.
JAR_DEST=`/bin/pwd`

# Many tool chains take care of the dependencies automatically.
# For this minimal build, dependencies must be handled explicitly.
# For ease in running on Mac, the UM dynamic libraries and JAR files
# are stored in $LBM_PLATFORM/lib.
UM_DEPS=$UM_JARS/UMS.jar:$JAR_DEST/lbmext.jar
TEST_DEPS=$UM_DEPS:$JUNIT:$HAMCREST:$JAR_DEST/lbmct.jar

# Clean out all the class and jar files.
find . -name '*.class' -print0 | xargs -0 rm -f
rm -f *.jar

# Build the logging extension to UM.

cd $JAR_DEST/main/com/latencybusters/lbm
$JAVA_HOME/javac -cp .:$UM_DEPS *.java
if [ $? -ne 0 ]; then exit; fi

cd ../../..
jar -cf $JAR_DEST/lbmext.jar com/latencybusters/lbm/*.class
if [ $? -ne 0 ]; then exit; fi

# Build lbmct.

cd $JAR_DEST/main/com/latencybusters/lbmct
$JAVA_HOME/javac -cp .:$UM_DEPS *.java
if [ $? -ne 0 ]; then exit; fi

cd ../../..
jar -cf $JAR_DEST/lbmct.jar com/latencybusters/lbmct/*.class
if [ $? -ne 0 ]; then exit; fi

# Build javadoc.

cd $JAR_DEST/main
$JAVA_HOME/javadoc -d ../../javadoc -quiet -cp ".:$UM_DEPS" -linksource com.latencybusters.lbmct

# Minimal examples

cd $JAR_DEST
$JAVA_HOME/javac -cp $JAR_DEST/lbmct.jar:$UM_DEPS MinCtSrc.java
if [ $? -ne 0 ]; then exit; fi

$JAVA_HOME/javac -cp $JAR_DEST/lbmct.jar:$UM_DEPS MinCtRcv.java
if [ $? -ne 0 ]; then exit; fi

$JAVA_HOME/java -cp .:$JAR_DEST/lbmct.jar:$UM_DEPS -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtRcv &
sleep 2
$JAVA_HOME/java -cp .:$JAR_DEST/lbmct.jar:$UM_DEPS -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtSrc

$JAVA_HOME/java -cp .:$JAR_DEST/lbmct.jar:$UM_DEPS -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtSrc &
sleep 2
$JAVA_HOME/java -cp .:$JAR_DEST/lbmct.jar:$UM_DEPS -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" MinCtRcv

# Test.  It uses junit, although the tests are not really unit tests.

cd $JAR_DEST/test/com/latencybusters/lbmct
$JAVA_HOME/javac -cp .:$TEST_DEPS *.java
if [ $? -ne 0 ]; then exit; fi

cd ../../..
jar -cf $JAR_DEST/test.jar com/latencybusters/lbmct/*.class
if [ $? -ne 0 ]; then exit; fi

cd $JAR_DEST/test/com/latencybusters/lbmct
$JAVA_HOME/java -cp $JAR_DEST/test.jar:$TEST_DEPS -Djava.library.path="$LBM_PLATFORM/lib" -Djava.security.egd="file:/dev/./urandom" org.junit.runner.JUnitCore com.latencybusters.lbmct.LbmCtTest
