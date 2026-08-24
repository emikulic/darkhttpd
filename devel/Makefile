.PHONY: all format clean

CCACHE := $(shell which ccache)
CCFLAGS ?= -g -O2 -fsanitize=address -fsanitize=undefined -Wall
COVFLAGS ?= $(CCFLAGS) --coverage -fno-omit-frame-pointer -DDEBUG -DAPBUF_INIT=1

SRCS := $(wildcard test_*.c)
OBJS := $(SRCS:.c=.o)
TESTS := $(SRCS:.c=)

all: $(TESTS) darkhttpd darkhttpd_no_debug darkhttpd_no_ipv6
	./run-tests

format:
	clang-format -i test_parse_field.c

$(OBJS): %.o: %.c ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -DDEBUG -c $<

# Linking is uncacheable.
$(TESTS): %: %.o
	$(CC) $(CCFLAGS) -o $@ $<

# Debug build with coverage instrumentation.
darkhttpd: ../darkhttpd.c
	$(CCACHE) $(CC) $(COVFLAGS) -c $< -o darkhttpd.o
	$(CC) $(COVFLAGS) darkhttpd.o -o darkhttpd

# Test building without -DDEBUG
darkhttpd_no_debug: ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -c $< -o darkhttpd_no_debug.o
	$(CC) $(CCFLAGS) -o $@ darkhttpd_no_debug.o

# Test building with -DNO_IPV6
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
		darkhttpd darkhttpd.o \
		darkhttpd_no_debug darkhttpd_no_debug.o \
		darkhttpd_no_ipv6 darkhttpd_no_ipv6.o \
		darkhttpd.gcda darkhttpd.gcno \
		fuzz_darkhttpd.o \
		fuzz_make_safe_uri \
		fuzz_socket \
		fuzz_parse_request
	rm -rf tmp.fuzz tmp.httpd.tests
