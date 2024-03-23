#define main darkhttpd_main
#include "../darkhttpd.c"
#undef main

char* sdup(const uint8_t *data, size_t size) {
  char *buf = malloc(size + 1);
  memcpy(buf, data, size);
  buf[size] = 0;
  return buf;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  wwwroot = sdup(data, size);
  char *path = get_wwwroot_parent();
  free(path);
  free(wwwroot);
  return 0;
}

/* vim:set ts=2 sw=2 sts=2 expandtab tw=78: */
