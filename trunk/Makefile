CFLAGS = -pipe -ansi -pedantic -g \
-Wall -Wredundant-decls -Wnested-externs -Wstrict-prototypes \
-Wpointer-arith -Winline -Wcast-qual -W \
-Wbad-function-cast -Wcast-align -Wchar-subscripts -Wshadow \
-Wwrite-strings -Wmissing-prototypes

all:
	gcc -o out $(CFLAGS) darkhttpd.c

splint:
	splint -skipposixheaders +posixlib +stats darkhttpd.c

