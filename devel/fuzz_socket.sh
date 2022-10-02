#!/bin/bash -e
set -x
mkdir -p fuzz_socket_testcases
clang -c -Dmain=darkhttpd -g -O2 -fsanitize=fuzzer,address ../darkhttpd.c -o fuzz_darkhttpd.o
clang++ -g -O2 -fsanitize=fuzzer,address fuzz_socket.cc fuzz_darkhttpd.o -o fuzz_socket
./fuzz_socket fuzz_socket_testcases -detect_leaks=0 -only_ascii=1

# Or run multiple processes on different ports with e.g.:
# env PORT=9999 ./fuzz_socket fuzz_socket_testcases -detect_leaks=0 -only_ascii=1
