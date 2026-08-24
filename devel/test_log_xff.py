#!/usr/bin/env python3
# This is run by the "run-tests" script.
import unittest
import os
import time
import random
import string
from test import TestHelper

LOG_FILE = "test_xff.log"

def random_str(n=10):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=n))

class TestTrustedIp(TestHelper):
    def _wait_and_check_log(self, unique_marker, timeout=2.0):
        """
        Polls the log file looking for a specific marker.
        Returns the specific log line containing the marker if found.
        """
        start = time.time()
        content = ""
        while time.time() - start < timeout:
            if os.path.exists(LOG_FILE):
                with open(LOG_FILE, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                    if unique_marker in content:
                        # Find and return only the specific line containing the unique marker
                        for line in content.splitlines():
                            if unique_marker in line:
                                return line
            time.sleep(0.1)
        # If we fail, print the content we did find to help debugging
        self.fail(f"Marker '{unique_marker}' not found in {LOG_FILE} within {timeout}s.\nLog content:\n{content}")


    def test_xff_single_ipv4(self):
        """
        Test that a single IPv4 address in X-Forwarded-For is logged.
        """
        fake_ip = "10.10.10.10"
        time_marker = random_str()
        url = f"/xff_single-{time_marker}"
        
        self.get(url, req_hdrs={"X-Forwarded-For": fake_ip})
        
        line = self._wait_and_check_log(url)
        # Log line format: IP - - [DATE] "GET ..."
        self.assertTrue(line.startswith(fake_ip + " "), 
            f"Expected log to start with {fake_ip}, but got: {line}")

    def test_xff_multiple_ipv4(self):
        """
        Test a comma-separated list of IPv4 addresses.
        The C code uses 'strchr' to find the first comma and effectively
        truncates the string there. We expect only the first IP to be logged.
        """
        client_ip = "1.2.3.4"
        proxy_ip = "5.6.7.8"
        # Standard XFF format: client, proxy1, proxy2
        header_val = f"{client_ip}, {proxy_ip}, {proxy_ip}"
        
        time_marker = random_str()
        url = f"/xff_multi-{time_marker}"
        
        self.get(url, req_hdrs={"X-Forwarded-For": header_val})
        
        line = self._wait_and_check_log(url)
        
        # Should start with the client IP
        self.assertTrue(line.startswith(client_ip + " "), 
            f"Expected log to start with first IP ({client_ip}). Log line: {line}")
        
        # Should NOT contain the proxy IP (it should be stripped)
        self.assertNotIn(proxy_ip, line, 
            "The second IP in the list should be stripped from the log.")

    def test_xff_ipv6(self):
        """
        Test a single IPv6 address. 
        Important to ensure logic doesn't break on colons (:).
        """
        ipv6 = "2001:db8::1"
        time_marker = random_str()
        url = f"/xff_ipv6-{time_marker}"
        
        self.get(url, req_hdrs={"X-Forwarded-For": ipv6})
        
        line = self._wait_and_check_log(url)
        self.assertTrue(line.startswith(ipv6 + " "), 
            f"Expected log to start with IPv6 {ipv6}. Log line: {line}")

    def test_xff_ipv6_list(self):
        """
        Test a list containing IPv6 and IPv4.
        Ensures the comma logic works when the first element is a long IPv6 string.
        """
        client_ipv6 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334"
        proxy_ip = "192.168.1.1"
        header_val = f"{client_ipv6}, {proxy_ip}"
        
        time_marker = random_str()
        url = f"/xff_ipv6_list-{time_marker}"
        
        self.get(url, req_hdrs={"X-Forwarded-For": header_val})
        
        line = self._wait_and_check_log(url)
        self.assertTrue(line.startswith(client_ipv6 + " "), 
            f"Expected log to start with {client_ipv6}. Log line: {line}")
        self.assertNotIn(proxy_ip, line)

if __name__ == '__main__':
    unittest.main()

# vim:set ts=4 sw=4 et: