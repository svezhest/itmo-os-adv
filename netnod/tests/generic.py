"""Generic test suite module."""
import logging
import os
import random
import re
import stat
import string
import subprocess
import unittest

from tests.constants import MODULE

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] %(filename)s:%(lineno)d: %(message)s'
)


class GenericTestCases(unittest.TestCase):
    """Generic tests, _send_and_assert should be replaced."""

    class NoModuleFound(Exception):
        """No module presented in current directory."""
        def __init__(self):
            super().__init__("Ensure that " + MODULE + ".ko is present in the current directory.")

    class OperationNotPermitted(Exception):
        """Not enough privileges."""
        def __init__(self):
            super().__init__("Ensure that tests are run with root privileges.")

    class UnexpectedError(Exception):
        """Some unexpected error."""

    class InvalidDevice(Exception):
        """Device doesn't match with expected characteristics."""

    module_loaded = False
    file_created = False

    @classmethod
    def setUpClass(cls):
        if not os.path.exists(MODULE + ".ko"):
            raise cls.NoModuleFound()

        try:
            subprocess.run(
                ["insmod", MODULE + ".ko"],
                check=True,
                stderr=subprocess.PIPE
            )
            cls.module_loaded = True
        except subprocess.CalledProcessError as exc:
            if b"Operation not permitted" in exc.stderr:
                raise cls.OperationNotPermitted() from exc
            elif b"File exists" in exc.stderr:
                # Module is already present, good job
                logging.info("Using already loaded module.")
            else:
                raise

        dmesg = subprocess.run(["dmesg"], check=True, stdout=subprocess.PIPE)
        majors = re.findall(
            MODULE.encode() + b" module loaded with device major number (\\d+)",
            dmesg.stdout
        )

        if len(majors) == 0:
            raise cls.UnexpectedError("No device major number found in dmesg.")

        major = int(majors[-1].decode())
        logging.info("Device major number: %d", major)

        if not os.path.exists("/dev/" + MODULE):
            # No device character file created, let's create one.
            cls.file_created = True
            os.mknod(
                "/dev/" + MODULE,
                mode=stat.S_IFCHR,
                device=os.makedev(major, 0)
            )

        stat_dev = os.stat("/dev/" + MODULE)
        if not stat.S_ISCHR(stat_dev.st_mode):
            raise cls.InvalidDevice("File is not a character device file.")

        if os.major(stat_dev.st_rdev) != major:
            raise cls.InvalidDevice("Major number doesn't match a one from dmesg.")

    @classmethod
    def tearDownClass(cls):
        if cls.file_created:
            try:
                os.remove("/dev/" + MODULE)
            except OSError as exc:
                logging.warning("Can't remove file", exc_info=exc)

        if cls.module_loaded:
            try:
                subprocess.run(["rmmod", MODULE], check=True, stderr=subprocess.PIPE)
            except subprocess.CalledProcessError as exc:
                logging.warning("Can't unload module", exc_info=exc)

    def _send_and_assert(self, value):
        raise NotImplementedError()

    def test_constants(self):
        """Checks small strings."""
        for value in ["y2019", "test", "a", "kjasdf"]:
            self._send_and_assert(value)

    def test_random(self):
        """Checks random strings."""
        for _ in range(10):
            testcase = ''.join(
                random.choice(string.ascii_letters)
                for _ in range(random.randint(5, 12))
            )

            self._send_and_assert(testcase)
