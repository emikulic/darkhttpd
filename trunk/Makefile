ALLWARNS = -Wall -Wshadow -Wpointer-arith -Wcast-qual \
-Wcast-align -Wwrite-strings -Wconversion -Waggregate-return \
-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations \
-Wredundant-decls -Wnested-externs -Wlong-long -Winline 

CFLAGS = -O -pipe -ansi -pedantic -g $(ALLWARNS) -DNDEBUG

all:
	gcc -o out $(CFLAGS) darkhttpd.c

splint:
	splint -weak -skipposixheaders +posixlib +stats darkhttpd.c
