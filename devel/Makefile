all:
	./run-tests

format:
	clang-format -i test_parse_field.c

clean:
	rm -f 	*.gcov \
		test.out.log \
		test.out.stderr \
		test.out.stdout \
		test.pyc \
		test_logging.log \
		test_xff.log \
		test_make_safe_uri \
		test_normalize_path \
		test_parse_field \
		test_password_equal \
		a.out \
		darkhttpd.gcda darkhttpd.gcno \
		a-darkhttpd.gcda a-darkhttpd.gcno \
		fuzz_darkhttpd.o \
		fuzz_make_safe_uri \
		fuzz_socket \
		fuzz_parse_request
	rm -rf tmp.fuzz tmp.httpd.tests
