#!/bin/sh
BDECFLAGS="-W -Wall -ansi -pedantic -Wbad-function-cast -Wcast-align \
-Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs \
-Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings"

(cc $BDECFLAGS -g -O -DDEBUG test_make_safe_uri.c -o test_make_safe_uri &&
./test_make_safe_uri)
