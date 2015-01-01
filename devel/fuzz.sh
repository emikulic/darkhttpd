#!/bin/bash -e
AFL_PATH=~/afl/afl-1.06b
export AFL_PATH
TMP=/dev/shm/darkhttpd
AFL_HARDEN=1 $AFL_PATH/afl-gcc -O3 -DDEBUG fuzz_make_safe_uri.c -o fuzz_make_safe_uri
mkdir $TMP
$AFL_PATH/afl-fuzz -i fuzz_testcases -o $TMP ./fuzz_make_safe_uri
