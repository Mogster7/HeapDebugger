#!/bin/evn bash

for i in `seq 0 11`;
do
    echo "./build-unix/HeapDebugger $i"
    ./build-unix/HeapDebugger $i
done
