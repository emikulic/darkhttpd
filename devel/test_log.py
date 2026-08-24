#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import os
import time
import random
import string
from test import TestHelper

LOG_FILE = "test_logging.log"

def random_str(n=10):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=n))

class TestLog(TestHelper):
    def _wait_and_check_log(self, unique_marker, timeout=2.0):
        """
        Polls the log file looking for a specific marker.
        Returns the content of the log file if found.
        """
        start = time.time()
        content = ""
        while time.time() - start < timeout:
            if os.path.exists(LOG_FILE):
                with open(LOG_FILE, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                    if unique_marker in content:
                        return content
            time.sleep(0.1)
        
        # If we fail, print the content we did find to help debugging
        self.fail(f"Marker '{unique_marker}' not found in {LOG_FILE} within {timeout}s.\nLog content:\n{content}")

    def test_log_sanitization_quotes(self):
        """
        Security Test: Log Injection Protection.
        Double quotes in headers (like Referer) must be hex-encoded (%22).
        If not encoded, an attacker could "break out" of the log format strings.
        """
        unique_id = random_str()
        # We use Referer because TestHelper might enforce its own User-Agent
        bad_referer = f'Ref-{unique_id} "hacker"'
        # Expectation: Quotes are replaced by %22
        expected_part = f'Ref-{unique_id} %22hacker%22'

        self.get("/sanit_quote", req_hdrs={"Referer": bad_referer})
        
        content = self._wait_and_check_log(unique_id)
        self.assertIn(expected_part, content, 
                      "Double quotes in Referer must be escaped as %22 to prevent log injection.")

    def test_log_truncation_newlines(self):
        """
        Security Test: Log Injection Protection via Newlines.
        
        darkhttpd parses headers by reading until \r or \n. 
        Therefore, if a client sends a header with a newline, darkhttpd 
        should stop parsing the value at that point.
        
        The result should be a truncated log entry, NOT a new log line (injection).
        """
        unique_id = random_str()
        injection_attempt = "127.0.0.1 - - [FAKE LOG ENTRY]"
        
        # We try to inject a newline into the Referer.
        # darkhttpd is expected to stop parsing at \n.
        bad_referer = f"Start-{unique_id}\n{injection_attempt}"
        
        self.get("/sanit_newline", req_hdrs={"Referer": bad_referer})

        content = self._wait_and_check_log(unique_id)
        
        # 1. Verify truncation: We should see the start, but NOT the injection attempt in the same string context
        # darkhttpd logic: splits header at \n. 'Referer' becomes just "Start-{unique_id}"
        self.assertIn(f"Start-{unique_id}", content)
        self.assertNotIn(injection_attempt, content, 
                         "The part after the newline should not appear in the log (header parsing should stop at newline).")
        
        # 2. Verify no duplicate unique_id (ensure the line didn't split into two valid looking lines)
        count = content.count(unique_id)
        self.assertEqual(count, 1, "Injection should not result in duplicate markers.")

    def test_empty_headers(self):
        """
        Test that empty headers are logged as empty quotes "" rather than skipped or malformed.
        """
        unique_url = f"/empty_hdrs_{random_str()}"
        
        # We explicitly set Referer to empty. 
        # Note: We don't test User-Agent here because TestHelper/Python requests 
        # usually force a default python UA if one isn't provided or is empty.
        self.get(unique_url, req_hdrs={"Referer": ""})
        
        content = self._wait_and_check_log(unique_url)
        
        found_line = False
        for line in content.splitlines():
            if unique_url in line:
                found_line = True
                # Format is: ... "REFERER" "USER_AGENT"
                # If Referer is empty, we expect: ... "" "..."
                self.assertIn('"" "', line, 
                              f"Empty Referer should be logged as \"\". Logged line: {line}")
                break
        
        self.assertTrue(found_line, "Could not find the specific log line for empty headers test.")

    def test_xff_ignored_by_default(self):
        """
        Security Test: X-Forwarded-For must be IGNORED by default.
        
        Unless the server is started with --trusted-ip, it must log the 
        actual connection IP (127.0.0.1), not the spoofed header.
        """
        fake_ip = "10.6.6.6"
        unique_url = f"/xff_test_{random_str()}"
        
        self.get(unique_url, req_hdrs={"X-Forwarded-For": fake_ip})
        
        content = self._wait_and_check_log(unique_url)
        
        target_line = ""
        for line in content.splitlines():
            if unique_url in line:
                target_line = line
                break
        
        self.assertTrue(target_line, "Log line not found for XFF test")
        
        # Log starts with IP. Check it is 127.0.0.1
        self.assertTrue(target_line.startswith("127.0.0.1"), 
                        f"Server must log real IP (127.0.0.1) by default. Logged: {target_line}")
        
        self.assertNotIn(fake_ip, target_line, 
                         "Spoofed X-Forwarded-For IP must not appear in log by default.")

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et:
