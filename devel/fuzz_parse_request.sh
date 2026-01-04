#!/bin/bash -e
set -x
mkdir -p fuzz_parse_request_testcases/
clang -g -O -fsanitize=fuzzer,address \
  fuzz_parse_request.c -o fuzz_parse_request
./fuzz_parse_request fuzz_parse_request_testcases/ > /dev/null
