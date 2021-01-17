#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
from test import TestHelper, Conn, parse

class TestForwardAll(TestHelper):
    def test_forward_root(self):
        resp = self.get('/', req_hdrs={'Host': 'not-example.com'})
        status, hdrs, body = parse(resp)
        self.assertContains(status, "301 Moved Permanently")
        expect = "http://catchall.example.com/"
        self.assertEqual(hdrs["Location"], expect)
        self.assertContains(body, expect)

    def test_forward_relative(self):
        resp = self.get('/foo/bar',
                req_hdrs={'Host': 'still-not.example.com'})
        status, hdrs, body = parse(resp)
        self.assertContains(status, "301 Moved Permanently")
        expect = "http://catchall.example.com/foo/bar"
        self.assertEqual(hdrs["Location"], expect)
        self.assertContains(body, expect)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
