#!/bin/bash -e
set -x
clang -g -O -fsanitize=fuzzer,address \
  fuzz_get_wwwroot_parent.c -o fuzz_get_wwwroot_parent
mkdir -p fuzz_get_wwwroot_parent_testcases
./fuzz_get_wwwroot_parent -only_ascii=1 $* fuzz_get_wwwroot_parent_testcases/
