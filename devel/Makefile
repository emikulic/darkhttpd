.PHONY: all format clean

CCACHE := $(shell which ccache)
CCFLAGS ?= -g -O2 -fsanitize=address -fsanitize=undefined -Wall

SRCS := $(wildcard test_*.c)
OBJS := $(SRCS:.c=.o)
TESTS := $(SRCS:.c=)

all: $(TESTS) darkhttpd_no_debug darkhttpd_no_ipv6
	./run-tests

format:
	clang-format -i test_parse_field.c

$(OBJS): %.o: %.c ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -c $<

# Linking is uncacheable.
$(TESTS): %: %.o
	$(CC) $(CCFLAGS) -o $@ $<

# Tests building without -DDEBUG
darkhttpd_no_debug: ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -c $< -o darkhttpd_no_debug.o
	$(CC) $(CCFLAGS) -o $@ darkhttpd_no_debug.o

# Tests building with -DNO_IPV6
darkhttpd_no_ipv6: ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -DNO_IPV6 -c $< -o darkhttpd_no_ipv6.o
	$(CC) $(CCFLAGS) -o $@ darkhttpd_no_ipv6.o

clean:
	rm -f 	*.gcov \
		test.out.log \
		test.out.stderr \
		test.out.stdout \
		test_logging.log \
		test_xff.log \
		$(OBJS) $(TESTS) \
		darkhttpd_no_debug darkhttpd_no_debug.o \
		darkhttpd_no_ipv6 darkhttpd_no_ipv6.o \
		a.out \
		darkhttpd.gcda darkhttpd.gcno \
		a-darkhttpd.gcda a-darkhttpd.gcno \
		fuzz_darkhttpd.o \
		fuzz_make_safe_uri \
		fuzz_socket \
		fuzz_parse_request
	rm -rf tmp.fuzz tmp.httpd.tests
