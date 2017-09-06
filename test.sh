#!/bin/bash
# unsplit testing script

dd if=/dev/urandom bs=512 count=500k of=test.tmp 2>/dev/null
rm -rf xa?
split -b 100M test.tmp
md5sum test.tmp > test.m5
./unsplit xa?
rm test.tmp
mv xaa test.tmp
md5sum -c test.m5 1>/dev/null
status=$?
rm -rf xa? test.tmp test.m5
exit $status
