all:
	@if [ "$$(id -u)" -eq 0 ]; then \
		chmod a+w "../devel";       \
		sudo -u nobody ./run-tests; \
	else                            \
		./run-tests;                \
	fi

clean:
	rm -f 	*.gcov \
		test.out.log \
		test.out.stderr \
		test.out.stdout \
		test.pyc \
		test_make_safe_uri \
		test_password_equal \
		a.out darkhttpd.gcda darkhttpd.gcno \
		fuzz_darkhttpd.o fuzz_llvm_make_safe_uri fuzz_socket
	rm -rf tmp.fuzz tmp.httpd.tests
