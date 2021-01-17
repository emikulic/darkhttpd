#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import os
import random
import base64
from test import WWWROOT, TestHelper, Conn, parse, random_bytes

class TestAuth(TestHelper):
    def setUp(self):
        self.datalen = 2345
        self.data = random_bytes(self.datalen)
        self.url = '/data.jpeg'
        self.fn = WWWROOT + self.url
        with open(self.fn, 'wb') as f:
            f.write(self.data)

    def tearDown(self):
        os.unlink(self.fn)

    def test_no_auth(self):
        resp = self.get(self.url)
        status, hdrs, body = parse(resp)
        self.assertContains(status, '401 Unauthorized')
        self.assertEqual(hdrs['WWW-Authenticate'],
            'Basic realm="User Visible Realm"')

    def test_with_auth(self):
        resp = self.get(self.url, req_hdrs={
            'Authorization':
            'Basic ' + base64.b64encode(b'myuser:mypass').decode('utf-8')})
        status, hdrs, body = parse(resp)
        self.assertContains(status, '200 OK')
        self.assertEqual(hdrs["Accept-Ranges"], "bytes")
        self.assertEqual(hdrs["Content-Length"], str(self.datalen))
        self.assertEqual(hdrs["Content-Type"], "image/jpeg")
        self.assertContains(hdrs["Server"], "darkhttpd/")
        assert body == self.data, [self.url, resp, status, hdrs, body]
        self.assertEqual(body, self.data)

    def test_wrong_auth(self):
        resp = self.get(self.url, req_hdrs={
            'Authorization':
            'Basic ' + base64.b64encode(b'myuser:wrongpass').decode('utf-8')})
        status, hdrs, body = parse(resp)
        self.assertContains(status, '401 Unauthorized')
        self.assertEqual(hdrs['WWW-Authenticate'],
            'Basic realm="User Visible Realm"')
        self.assertContains(hdrs['Server'], 'darkhttpd/')

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
