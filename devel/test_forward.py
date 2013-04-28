#!/usr/bin/env python
import unittest
from test import TestHelper, Conn, parse

class TestForward(TestHelper):
    def test_forward_root(self):
        resp = Conn().get("/", req_hdrs = { "Host": "example.com" })
        status, hdrs, body = parse(resp)
        self.assertContains(status, "301 Moved Permanently")
        expect = "http://www.example.com/"
        self.assertEquals(hdrs["Location"], expect)
        self.assertContains(body, expect)

    def test_forward_relative(self):
        resp = Conn().get("/foo/bar",
                          req_hdrs = { "Host": "secure.example.com" })
        status, hdrs, body = parse(resp)
        self.assertContains(status, "301 Moved Permanently")
        expect = "https://www.example.com/secure/foo/bar"
        self.assertEquals(hdrs["Location"], expect)
        self.assertContains(body, expect)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
