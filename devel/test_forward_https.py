#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
from test import TestHelper, Conn, parse

class TestForward(TestHelper):
    def test_without_header(self):
        resp = self.get('/', req_hdrs={'Host': 'example.com'})
        status, hdrs, body = parse(resp)
        self.assertContains(status, '200 OK')

    def test_https_redirect(self):
        resp = self.get('/foo/bar', req_hdrs={
            'Host': 'example.com',
            'X-Forwarded-Proto': 'http',
        })
        status, hdrs, body = parse(resp)
        self.assertContains(status, '301 Moved Permanently')
        expect = 'https://example.com/foo/bar'
        self.assertEqual(hdrs['Location'], expect)
        self.assertContains(body, expect)

    def test_no_redirect(self):
        resp = self.get('/', req_hdrs={
            'Host': 'example.com',
            'X-Forwarded-Proto': 'https',  # Already https.
        })
        status, hdrs, body = parse(resp)
        self.assertContains(status, '200 OK')

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
