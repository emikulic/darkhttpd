#!/usr/bin/env python
import sys, socket

request = (
'GET /darkhttpd.c HTTP/1.0\r\n'
'\r\n'
)

s = socket.socket()
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
s.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 1)
#s.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1)
#^ for some reason, this un-cripples the receiving buffer
try:
    s.connect(("", 8089))
except socket.error, e:
    print "ERROR: darkhttpd not running?"
    print "Run: cd trunk && ./darkhttpd . --port 8089"
    print ""
    raise e

print "(start sending)"

for i in request:
    numsent = s.send(i)
    if numsent != 1:
        raise Exception, "couldn't send"
    sys.stdout.write(i)
    sys.stdout.flush()

print "(done sending - start receiving)"

while True:
    c = s.recv(1)
    if c == '':
        print "(done receiving)"
        break
    sys.stdout.write(c)
    sys.stdout.flush()

# vim:set sw=4 ts=4 et tw=78:
