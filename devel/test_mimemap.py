#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import os
from test import WWWROOT, TestHelper, Conn, parse

class TestMimemap(TestHelper):
    def setUp(self):
        self.data = b'hello\n'
        self.datalen = len(self.data)
        self.files = [ ("test-file.a1",    "test/type1"),
                       ("test-file.ap2",   "test/type2"),
                       ("test-file.app3",  "test/type3"),
                       ("test-file.appp4", "test/default") ]
        for fn, _ in self.files:
            with open(WWWROOT + "/" + fn, 'wb') as f:
                f.write(self.data)

    def tearDown(self):
        for fn, _ in self.files:
            os.unlink(WWWROOT + "/" + fn)

    def get_helper(self, idx):
        fn, content_type = self.files[idx]
        resp = self.get("/" + fn)
        status, hdrs, body = parse(resp)
        self.assertContains(status, "200 OK")
        self.assertEqual(hdrs["Accept-Ranges"], "bytes")
        self.assertEqual(hdrs["Content-Length"], str(self.datalen))
        self.assertEqual(hdrs["Content-Type"], content_type)
        self.assertContains(hdrs["Server"], "darkhttpd/")
        self.assertEqual(body, self.data)

    def test_get_1(self): self.get_helper(0)
    def test_get_2(self): self.get_helper(1)
    def test_get_3(self): self.get_helper(2)
    def test_get_4(self): self.get_helper(3)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
