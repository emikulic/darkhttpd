#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static void
test(int equal, const char *user_input, const char *secret)
{
    int out = password_equal(user_input, secret);
    printf("%s: \"%s\" \"%s\"\n",
        (equal == out) ? "PASS" : "FAIL",
        user_input, secret);
}

int
main(void)
{
    test(1, "", "");
    test(1, "a", "a");
    test(1, "abc", "abc");

    test(0, "a", "");
    test(0, "ab", "");
    test(0, "", "a");
    test(0, "", "ab");
    test(0, "abcd", "abc");
    test(0, "abc", "abcd");
    return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
