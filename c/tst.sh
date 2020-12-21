#!/bin/sh
# tst.sh

D=`pwd`
cd ..
. ./lbm.sh
cd "$D"

#./min_ct_rcv &
#./min_ct_src

#sleep 1

./test $* --gtest_break_on_failure
