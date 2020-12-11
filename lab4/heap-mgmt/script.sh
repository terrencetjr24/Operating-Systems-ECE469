#!/bin/bash

cd os/
make clean
make

cd ../apps/given_example/
make clean
make
make run
cd ../..
