#!/bin/bash -e
set -x
clang -g -O -fsanitize=fuzzer,address \
  fuzz_make_safe_uri.c -o fuzz_make_safe_uri
./fuzz_make_safe_uri -only_ascii=1 $* fuzz_make_safe_uri_testcases/
