#!/bin/bash
rm testoutput

cd os/
make clean
make


cd ../apps/multi_fork_call
make clean
make
make run > testoutput
mv testoutput ../../
cd ../..
less testoutput
