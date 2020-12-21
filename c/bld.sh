#!/bin/sh
# bld.sh

D=`pwd`
cd ..
. ./lbm.sh
cd "$D"

rm -f *.o

gcc $* -Werror -Wall -I$LBM_PLATFORM/include -I$GTEST/include -g -c lbmct*.c tmr.c err.c sb.c
G_STAT=$?
if [ $G_STAT -ne 0 ]; then exit; fi

g++ $* -I$LBM_PLATFORM/include -I$GTEST/include -L$LBM_PLATFORM/lib -llbm -lm -g -o test main.cc *.o $GTEST/libgtest.a
G_STAT=$?
if [ $G_STAT -ne 0 ]; then exit; fi

#gcc $* -Werror -Wall -I$LBM_PLATFORM/include -L$LBM_PLATFORM/lib -llbm -lm -g -o min_ct_rcv min_ct_rcv.c *.o
G_STAT=$?
if [ $G_STAT -ne 0 ]; then exit; fi

#gcc $* -Werror -Wall -I$LBM_PLATFORM/include -L$LBM_PLATFORM/lib -llbm -lm -g -o min_ct_src min_ct_src.c *.o
G_STAT=$?
if [ $G_STAT -ne 0 ]; then exit; fi
