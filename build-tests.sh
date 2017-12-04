#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TEST_DIR="${DIR}/test"
TEST_FILES="${TEST_DIR}/*.c"
error=0

echo "Building test files in ${TEST_DIR} ..."

for f in $TEST_FILES
do
    in=$f
    out="${in%.*}.ll"
    echo "Compiling $(basename "${in}") ..."

    clang -S -O0 -g -emit-llvm "$in" -o "$out"

    if [ $? -ne 0 ]; then
        error=$((error + 1))
        echo "Error while compiling $(basename "${in}")!"
    else
        echo "Generated $(basename "${out}") ."
    fi
done

if [ $error -ne 0 ]; then
    echo "Failed with ${error} errors."
else
    echo "Done."
fi
