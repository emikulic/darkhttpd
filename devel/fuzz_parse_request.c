/* Wrapper around parse_request() for fuzzing. */
#define main darkhttpd_main
#include "../darkhttpd.c"
#undef main

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  /* Enable some server options to exercise more code paths. */
  trusted_ip = "0.0.0.0";
  logfile = stdout;

  struct connection *conn = new_connection();

  /* Copy fuzzer's data, append \n\n and terminating NUL. */
  conn->request = xmalloc(size + 3);
  memcpy(conn->request, data, size);
  conn->request[size+0] = '\n';
  conn->request[size+1] = '\n';
  conn->request[size+2] = '\0';
  conn->request_length = size + 3;
  conn->state = RECV_REQUEST;

  /* This is what we're fuzzing. */
  if (parse_request(conn)) {
    /* Extra code feature for successful parses: pretend to advance state. */
    conn->state = SEND_HEADER;
  }

  /* Valid code causes log_connection to do work. */
  conn->http_code = 200;

  free_connection(conn);
  free(conn);
  return 0;
}

/* vim:set ts=2 sw=2 sts=2 expandtab tw=78: */
