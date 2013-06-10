#!/usr/bin/env python
# This is run by the "cover" script.
import unittest
import os
from test import WWWROOT, TestHelper, Conn, parse

class TestMimemap(TestHelper):
    def setUp(self):
        self.data = "hello\n"
        self.datalen = len(self.data)
        self.files = [ ("test-file.a1",    "test/type1"),
                       ("test-file.ap2",   "test/type2"),
                       ("test-file.app3",  "test/type3"),
                       ("test-file.appp4", "application/octet-stream") ]
        for fn, _ in self.files:
            open(WWWROOT + "/" + fn, "w").write(self.data)

    def tearDown(self):
        for fn, _ in self.files:
            os.unlink(WWWROOT + "/" + fn)

    def get_helper(self, idx):
        fn, content_type = self.files[idx]
        resp = Conn().get("/" + fn)
        status, hdrs, body = parse(resp)
        self.assertContains(status, "200 OK")
        self.assertEquals(hdrs["Accept-Ranges"], "bytes")
        self.assertEquals(hdrs["Content-Length"], str(self.datalen))
        self.assertEquals(hdrs["Content-Type"], content_type)
        self.assertContains(hdrs["Server"], "darkhttpd/")
        self.assertEquals(body, self.data)

    def test_get_1(self): self.get_helper(0)
    def test_get_2(self): self.get_helper(1)
    def test_get_3(self): self.get_helper(2)
    def test_get_4(self): self.get_helper(3)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
