#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static void
test(const char *input, const char *expected)
{
    char *tmp = NULL;
    if (input!=NULL) tmp = xstrdup(input);
    char *out = normalize_path(tmp);

    if (expected == NULL) {
        if (out == NULL)
            printf("PASS: \"%s\" is normalized\n", input);
        else
            printf("FAIL: \"%s\" is normalized, but got \"%s\"\n",
                input, out);
    }
    else if (out == NULL)
        printf("FAIL: \"%s\" should become \"%s\", got NULL\n",
            input, expected);
    else if (strcmp(out, expected) == 0)
        printf("PASS: \"%s\" => \"%s\"\n", input, out);
    else
        printf("FAIL: \"%s\" => \"%s\", expecting \"%s\"\n",
            input, out, expected);
    free(tmp);
}

static char const *tests[] = {
    NULL, NULL,
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
    NULL
};

int
main(void)
{
    const char **curr = tests;

    do {
        test(curr[0], curr[1]);
        curr += 2;
    } while (curr[0] != NULL);

    return 0;
}
/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
