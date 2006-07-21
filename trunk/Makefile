# darkhttpd (c) 2003, 2004, 2006 Emil Mikulic.

CC?=cc
CFLAGS?=-O

TARGETS = bsd linux solaris
.PHONY: all $(TARGETS)

all:
	@echo "+----- ---- --- -- -"
	@echo "| Please run:"
	@echo "|     make <target>"
	@echo "| Where target is one of the following:"
	@echo "|     $(TARGETS)"
	@echo "+----- ---- --- -- -"

bsd:
	make darkhttpd

linux:
	make darkhttpd

solaris:
	make darkhttpd LIBS="-lxnet -lsendfile"

darkhttpd: darkhttpd.c
	$(CC) $(CFLAGS) $(LIBS) darkhttpd.c -o darkhttpd

clean:
	rm -f darkhttpd core darkhttpd.core
