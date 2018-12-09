#!/usr/bin/env python3
# Opens sockets until they run out.
import sys, socket

def main():
  request = b'GET /darkhttpd.c HTTP/1.0\r\n'
  socks = []
  print('Trying to connect...')
  first = True
  while True:
    try:
      s = socket.socket()
      s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
      s.connect(("", 8080))
      s.send(request)
      socks.append(s)
      print(len(socks), 'sockets open')
    except Exception as e:
      if first:
        print(e)
        first = False
      pass

if __name__ == '__main__': main()

# vim:set sw=2 ts=2 sts=2 et tw=80:
