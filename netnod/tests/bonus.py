"""Tests for the bonus task."""

import hashlib

from tests.constants import MODULE
from tests.generic import GenericTestCases


class BonusTestCases(GenericTestCases):
    """Tests for the bonus task."""
    def _send_and_assert(self, value):
        with open("/dev/" + MODULE, "w") as fout:
            fout.write(value)

        with open("/dev/" + MODULE, "r") as fin:
            actual = fin.read().strip()

        expected = hashlib.sha512(value.encode()).hexdigest()
        self.assertEqual(actual, expected)
