#!/usr/bin/env python
# This is run by the "cover" script.
import unittest
from test import TestHelper, Conn, parse

class TestNoListing(TestHelper):
    def test_no_listing(self):
        resp = self.get("/")
        status, hdrs, body = parse(resp)
        self.assertContains(status, "404 Not Found")

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
