#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import signal
import socket

class TestTimeout(unittest.TestCase):
    def test_timeout(self):
        port = 12346
        s = socket.socket()
        s.connect(("0.0.0.0", port))
        # Assumes the server has --timeout 1
        signal.alarm(3)
        # Expect to get EOF before the alarm fires.
        ret = s.recv(1024)
        signal.alarm(0)
        s.close()
        self.assertEqual(ret, b'')

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
