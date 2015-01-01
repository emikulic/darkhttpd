// Wrapper around make_safe_url() for fuzzing.
// Aborts if the output is deemed safe but contains /../ or /./
#include <stdio.h>

#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

int main(void) {
    char *buf = NULL;
    size_t len = 0;
    ssize_t num_read = getline(&buf, &len, stdin);
    if (num_read == -1) return 1;
    int l = strlen(buf);
    if (l > 0) {
        buf[l-1] = '\0';
    }
    char* safe = make_safe_url(buf);
    if (safe) {
        if (strstr(safe, "/../") != NULL) abort();
        if (strstr(safe, "/./") != NULL) abort();
    }
    return 0;
}
/* vim:set ts=4 sw=4 sts=4 expandtab tw=78: */
