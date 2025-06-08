#!/bin/bash -e
set -x
mkdir -p fuzz_socket_testcases
clang -c -DDEBUG -Dmain=darkhttpd -g -O2 -fsanitize=fuzzer,address ../darkhttpd.c -o fuzz_darkhttpd.o
clang++ -g -O2 -DDEBUG -fsanitize=fuzzer,address fuzz_socket.cc fuzz_darkhttpd.o -o fuzz_socket
mkdir -p tmp.fuzz
echo hello > tmp.fuzz/hello.txt
./fuzz_socket fuzz_socket_testcases -detect_leaks=0 >/dev/null

# Or run multiple processes on different ports with e.g.:
# env PORT=9999 ./fuzz_socket fuzz_socket_testcases -detect_leaks=0 -only_ascii=1
