/* darkhttpd benchmark.
 * https://unix4lyfe.org/darkhttpd/
 * Copyright (c) 2018 Emil Mikulic <emikulic@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static const char req[] = "GET /../ HTTP/1.0\r\n\r\n";
static const int req_len = sizeof(req) - 1;

int main(int argc, char **argv) {
  int fd;
  const char *host = "127.0.0.1";
  int port = 8080;
  struct sockaddr_in addrin;
  char buf[2096];
  ssize_t rcvd, sent;
  struct timespec t0, t1;
  int every = 10;
  int count = 0;

  addrin.sin_family = AF_INET;
  addrin.sin_port = htons(port);
  if (inet_aton(host, &addrin.sin_addr) == 0) err(1, "inet_aton");

  clock_gettime(CLOCK_MONOTONIC, &t0);
  while (1) {
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) err(1, "socket");

    if (connect(fd, (const struct sockaddr *)&addrin,
                sizeof(struct sockaddr)) == -1)
      err(1, "connect");

    sent = send(fd, req, req_len, 0);
    if (sent != req_len) err(1, "send");
    rcvd = recv(fd, buf, sizeof(buf), 0);
    if (rcvd == -1) err(1, "recv");
    if (rcvd == 0) errx(1, "eof");
    if (rcvd == sizeof(buf)) errx(1, "long response");
    close(fd);

    count++;
    if (count >= every) {
      double dt;
      double qps;

      clock_gettime(CLOCK_MONOTONIC, &t1);
      dt = t1.tv_sec - t0.tv_sec;
      dt += (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;
      qps = count / dt;
      printf("%f qps\n", qps);
      fflush(stdout);

      every = (every / 2) + (int)(qps / 2);
      t0 = t1;
      count = 0;
    }
  }

  return 0;
}

/* vim:set ts=2 sw=2 sts=2 expandtab tw=80: */
