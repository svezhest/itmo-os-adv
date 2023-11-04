"""Tests for the main task."""

import hashlib
import socket

from tests.constants import FILENAME, MODULE, REMOTE
from tests.generic import GenericTestCases


class BasicTestCases(GenericTestCases):
    """Tests for the main task."""
    def _send_and_assert(self, value):
        with open("/dev/" + MODULE, "w") as fout:
            fout.write(value)

        with open("/dev/" + MODULE, "r") as fin:
            fin.read()

        with open(FILENAME, "r") as ans:
            actual = ans.read().strip()

        expected = hashlib.sha512(value.encode()).hexdigest()
        self.assertEqual(actual, expected)

    def test_port_open(self):
        """Checks port is open."""
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect(REMOTE)
