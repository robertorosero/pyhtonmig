#!/bin/sh

if ! [ -d testdata ]; then
    mkdir testdata
fi

if ! [ -f testdata/add.decTest ]; then
    cp ../../decimaltestdata/* testdata
    mv testdata/randomBound32.decTest testdata/randombound32.decTest 
    mv testdata/remainderNear.decTest testdata/remaindernear.decTest 
fi

if ! [ -f testdata/baseconv.decTest ]; then
    cp testdata_dist/* testdata
fi

