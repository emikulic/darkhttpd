BDECFLAGS="-W -Wall -ansi -pedantic -Wbad-function-cast -Wcast-align \
-Wcast-qual -Wchar-subscripts -Winline -Wmissing-prototypes -Wnested-externs \
-Wpointer-arith -Wredundant-decls -Wshadow -Wstrict-prototypes -Wwrite-strings"

(cd trunk &&
rm -f darkhttpd &&
make bsd CFLAGS="$BDECFLAGS -g -DDEBUG -DTORTURE")
