CC?=cc
CFLAGS?=-O
LIBS=`[ \`uname\` = "SunOS" ] && echo -lsocket -lnsl`

all: darkhttpd

darkhttpd: darkhttpd.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) darkhttpd.c -o $@

clean:
	rm -f darkhttpd core darkhttpd.core

.PHONY: all clean
