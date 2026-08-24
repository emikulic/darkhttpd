.PHONY: all format clean

CCACHE := $(shell which ccache)
CCFLAGS ?= -g -O2 -fsanitize=address -fsanitize=undefined

SRCS := $(wildcard test_*.c)
OBJS := $(SRCS:.c=.o)
TESTS := $(SRCS:.c=)

all: $(TESTS)
	./run-tests

format:
	clang-format -i test_parse_field.c

$(OBJS): %.o: %.c ../darkhttpd.c
	$(CCACHE) $(CC) $(CCFLAGS) -c $<

# Linking is uncacheable.
$(TESTS): %: %.o
	$(CC) $(CCFLAGS) -o $@ $<

clean:
	rm -f 	*.gcov \
		test.out.log \
		test.out.stderr \
		test.out.stdout \
		test_logging.log \
		test_xff.log \
		$(OBJS) $(TESTS) \
		a.out \
		darkhttpd.gcda darkhttpd.gcno \
		a-darkhttpd.gcda a-darkhttpd.gcno \
		fuzz_darkhttpd.o \
		fuzz_make_safe_uri \
		fuzz_socket \
		fuzz_parse_request
	rm -rf tmp.fuzz tmp.httpd.tests
