#define main _main_disabled_
#include "../darkhttpd.c"
#undef main

static void test(const char* input_wwwroot, const char* expected_path,
                 const char* expected_wwwroot) {
  wwwroot = xstrdup(input_wwwroot);
  char* path = get_wwwroot_parent();
  int pass = (strcmp(path, expected_path) == 0) &&
             (strcmp(wwwroot, expected_wwwroot) == 0);
  printf("%s: \"%s\" -> \"%s\" : \"%s\"", pass ? "PASS" : "FAIL", input_wwwroot,
         path, wwwroot);
  if (!pass) {
    printf(" (expected \"%s\" : \"%s\")", expected_path, expected_wwwroot);
  }
  printf("\n");
  free(path);
  free(wwwroot);
}

int main(void) {
  test("", ".", "");
  test(".", ".", ".");
  test("dir/file", "dir/", "/file");
  test("./a/b/c", "./a/b/", "/c");
  return 0;
}

/* vim:set tabstop=4 shiftwidth=4 expandtab tw=78: */
