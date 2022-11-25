#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import os
from test import WWWROOT, TestHelper, parse, random_bytes

class TestCustomHeaders(TestHelper):
    def setUp(self):
        self.datalen = 2345
        self.data = random_bytes(self.datalen)
        self.url = '/data.jpeg'
        self.fn = WWWROOT + self.url
        with open(self.fn, 'wb') as f:
            f.write(self.data)

    def tearDown(self):
        os.unlink(self.fn)

    def test_custom_headers(self):
        resp = self.get(self.url)
        status, hdrs, body = parse(resp)
        self.assertContains(status, '200 OK')
        self.assertEqual(hdrs["Accept-Ranges"], "bytes")
        self.assertEqual(hdrs["Content-Length"], str(self.datalen))
        self.assertEqual(hdrs["Content-Type"], "image/jpeg")
        self.assertEqual(hdrs["X-Header-A"], "First Value")
        self.assertEqual(hdrs["X-Header-B"], "Second Value")
        self.assertContains(hdrs["Server"], "darkhttpd/")
        assert body == self.data, [self.url, resp, status, hdrs, body]
        self.assertEqual(body, self.data)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
