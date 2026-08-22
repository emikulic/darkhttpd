#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static int ret = EXIT_SUCCESS;

#define test(a, b) test_helper(__FILE__, __LINE__, a, b)

static void test_helper(const char *file, int line, const char *expected,
                        const char *actual) {
  if (actual == NULL && expected == NULL) {
    printf("%s:%d:PASS\n", file, line);
    return;
  }
  if (actual == NULL) {
    printf("%s:%d:FAIL: got NULL, expected \"%s\"\n", file, line, expected);
    ret = EXIT_FAILURE;
    return;
  }
  if (expected == NULL) {
    printf("%s:%d:FAIL: got \"%s\", expected NULL\n", file, line, actual);
    ret = EXIT_FAILURE;
    return;
  }
  if (strcmp(expected, actual) != 0) {
    printf("%s:%d:FAIL: got \"%s\", expected \"%s\"\n", file, line, actual,
           expected);
    ret = EXIT_FAILURE;
    return;
  }
  printf("%s:%d:PASS: \"%s\"\n", file, line, actual);
}

char *parse(const char *req) {
  struct connection conn;
  conn.request = (char *)req;
  conn.request_length = strlen(conn.request);
  return parse_field(&conn, "Host: ");
}

int main(void) {
  {
    // Correct Host header.
    char *s = parse("GET / HTTP/1.0\r\n"
                    "Host: example.com\r\n"
                    "\r\n");
    test("example.com", s);
    if (s)
      free(s);
  }

  {
    // Header at start of request: make sure parser doesn't access request[-1].
    char *s = parse("Host: example.com\r\n");
    test("example.com", s);
    if (s)
      free(s);
  }

  {
    // Don't find header inside another header.
    char *s = parse("GET / HTTP/1.0\r\n"
                    "Referer: http://referer.com/ Host: wrong.com\r\n"
                    "\r\n");
    test(NULL, s);
    if (s)
      free(s);
  }

  {
    // Don't find header inside another header.
    // We accept non-standard LF-only line endings.
    char *s = parse("GET / HTTP/1.0\n"
                    "Referer: http://referer.com/ Host: wrong.com\n"
                    "\n");
    test(NULL, s);
    if (s)
      free(s);
  }

  {
    // Find valid header after injection attempt.
    char *s = parse("GET / HTTP/1.0\r\n"
                    "Referer: http://referer.com/ Host: wrong.com\r\n"
                    "Host: right.com\r\n"
                    "\r\n");
    test("right.com", s);
    if (s)
      free(s);
  }

  return ret;
}
