#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static void test(const char *input, const char *expected) {
    char* actual = xstrdup(input);
    normalize_path(actual);
    if (strcmp(actual, expected) == 0)
        printf("PASS: \"%s\" => \"%s\"\n", input, actual);
    else
        printf("FAIL: \"%s\" => \"%s\", expecting \"%s\"\n",
            input, actual, expected);
    free(actual);
}

static char const *tests[] = {
    "", "",
    "/", "/",
    "/.", "/.",
    "/./", "/./",
    "/.d", "/.d",
    "/abc/..", "/abc/..",
    "http://", "",
    "http://a", "",
    "http://a/", "/",
    "http://a/index.htm", "/index.htm",
    "https://", "",
    "https://a", "",
    "https://a/", "/",
    "https://a/index.htm", "/index.htm",
    "https://example.com:12345/index.htm", "/index.htm",
    NULL
};

int main(void) {
    const char **curr = tests;

    do {
        test(curr[0], curr[1]);
        curr += 2;
    } while (curr[0] != NULL);

    return 0;
}
/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
