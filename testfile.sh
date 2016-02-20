#!/bin/bash

echo -n > test.log

SHARCXX=./sharcxx.exe
SHARC=../learning/density/sharc.exe

test_it() {
    origfn=$1
    comp=$2
    if test -e $origfn.$comp.sharc; then rm $origfn.$comp.sharc; fi
    cp $origfn $origfn.$comp
    valgrind --leak-check=full $SHARCXX -$comp $origfn.$comp 2>> test.log
    rm $origfn.$comp
    if valgrind --leak-check=full $SHARCXX -d $origfn.$comp.sharc 2>> test.log; then
        if cmp $origfn $origfn.$comp; then
            echo $comp succ
        else
            echo $comp fail0
        fi
    else
        if test -e $origfn.$comp; then rm $origfn.$comp; fi
        $SHARC -d $origfn.$comp.sharc
        if cmp $origfn $origfn.$comp; then
            echo $comp compress succ, decompress fail.
        else
            echo $comp fail1
        fi
    fi
}

test_it $1 c0
test_it $1 c1
test_it $1 c2
test_it $1 c3
