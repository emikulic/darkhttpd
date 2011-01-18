#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static void
test(const char *input, const char *expected)
{
    char *tmp = xstrdup(input);
    char *out = make_safe_uri(tmp);

    if (expected == NULL) {
        if (out == NULL)
            printf("PASS: \"%s\" is unsafe\n", input);
        else
            printf("FAIL: \"%s\" is unsafe, but got \"%s\"\n",
                input, out);
    }
    else if (out == NULL)
        printf("FAIL: \"%s\" should become \"%s\", got unsafe\n",
            input, expected);
    else if (strcmp(out, expected) == 0)
        printf("PASS: \"%s\" => \"%s\"\n", input, out);
    else
        printf("FAIL: \"%s\" => \"%s\", expecting \"%s\"\n",
            input, out, expected);
    free(tmp);
}

static char const *tests[] = {
    "/", "/",
    "/.", "/",
    "/./", "/",
    "/../", NULL,
    "/abc", "/abc",
    "/abc/", "/abc/",
    "/abc/.", "/abc",
    "/abc/./", "/abc/",
    "/abc/..", "/",
    "/abc/../", "/",
    "/abc/../def", "/def",
    "/abc/../def/", "/def/",
    "/abc/../def/..", "/",
    "/abc/../def/../", "/",
    "/abc/../def/../../", NULL,
    "/abc/../def/.././", "/",
    "/abc/../def/.././../", NULL,
    "/a/b/c/../../d/", "/a/d/",
    "/a/b/../../../c", NULL,
    /* don't forget consolidate_slashes */
    "//a///b////c/////", "/a/b/c/",
    NULL
};

int
main(void)
{
    const char **curr = tests;

    while (curr[0] != NULL) {
        test(curr[0], curr[1]);
        curr += 2;
    }

    return 0;
}
/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
