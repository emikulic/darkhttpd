#!/usr/bin/env python
# This is run by the "run-tests" script.
import unittest
import os
import random
import base64
from test import WWWROOT, TestHelper, Conn, parse

class TestAuth(TestHelper):
    def setUp(self):
        self.datalen = 2345
        self.data = ''.join(
            [chr(random.randint(0,255)) for _ in xrange(self.datalen)])
        self.url = '/data.jpeg'
        self.fn = WWWROOT + self.url
        with open(self.fn, 'w') as f:
            f.write(self.data)

    def tearDown(self):
        os.unlink(self.fn)

    def test_no_auth(self):
        resp = self.get(self.url)
        status, hdrs, body = parse(resp)
        self.assertContains(status, '401 Unauthorized')
        self.assertEquals(hdrs['WWW-Authenticate'],
            'Basic realm="User Visible Realm"')

    def test_with_auth(self):
        resp = self.get(self.url, req_hdrs={
            'Authorization': 'Basic '+base64.b64encode('myuser:mypass')})
        status, hdrs, body = parse(resp)
        self.assertContains(status, '200 OK')
        self.assertEquals(hdrs["Accept-Ranges"], "bytes")
        self.assertEquals(hdrs["Content-Length"], str(self.datalen))
        self.assertEquals(hdrs["Content-Type"], "image/jpeg")
        self.assertContains(hdrs["Server"], "darkhttpd/")
        assert body == self.data, [url, resp, status, hdrs, body]
        self.assertEquals(body, self.data)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
