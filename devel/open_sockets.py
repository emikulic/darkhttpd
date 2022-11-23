#!/usr/bin/env python3
# Opens sockets until they run out.
import socket
from time import time

def main():
  request = b'GET /darkhttpd.c HTTP/1.0\r\n'
  socks = []
  print('Trying to connect...')
  first = True
  while True:
    try:
      t0 = time(); s = socket.socket()
      t1 = time(); s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
      t2 = time(); s.connect(("", 8080))
      t3 = time(); s.send(request)
      t4 = time(); socks.append(s)
      t5 = time()
      print('%d sockets open, %f sock, %f setsockopt, %f connect, %f send, %f append' % (
        len(socks), t1-t0, t2-t1, t3-t2, t4-t3, t5-t4))
    except Exception as e:
      if first:
        print(e)
        first = False
      pass

if __name__ == '__main__': main()

# vim:set sw=2 ts=2 sts=2 et tw=80:
