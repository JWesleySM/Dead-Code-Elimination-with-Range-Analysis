#!/bin/bash

###############################################################################
# This file runs the range analysis on the program that it receives as input.
# Author: Fernando Magno Quintao Pereira
# Date: March 3rd, 2016
# Usage: run_RA.sh file.c 
###############################################################################

if [ $# -lt 1 ]
then
    echo Syntax: run_RA file.c
    exit 1
else
    CLANG="/home/jw/llvm/build/bin/clang"
    OPT="/home/jw/llvm/build/bin/opt"
    RANGE_LIB="/home/jw/llvm/build/lib/RangeAnalysis.so"

    file_name=$1
    base_name=$(basename $1 .c)
    orig_btcd_name="$base_name.orig.bc"
    new_btcd_name="$base_name.rbc"
    vssa_btcd_name="$base_name.vssa.rbc"
    new_cfg_name="$base_name.new.bc"

    # Produce a bytecode file:
    $CLANG $file_name -o $orig_btcd_name -c -emit-llvm -Xclang -disable-O0-optnone

    # Convert the bytecode to SSA form, find nice names for variables and
    # break critical edges:
    $OPT -instnamer -mem2reg -break-crit-edges $orig_btcd_name -o $new_btcd_name

    # Break live ranges after conditionals to improve precision:
    $OPT -load $RANGE_LIB -vssa $new_btcd_name -o $vssa_btcd_name

    # Run the range analysis client    
    $OPT -load $RANGE_LIB -dcewra $vssa_btcd_name -o $new_cfg_name -stats

    # Producing a dot file for the vssa version of the bytecode file:
    $OPT -dot-cfg $new_cfg_name -disable-output

    # Remove the temporary files produced. We keep the '.s' file if the option
    # -s was passed in the command line.
    rm -f $orig_btcd_name
    rm -f $new_btcd_name
    rm -f $vssa_btcd_name
fi
