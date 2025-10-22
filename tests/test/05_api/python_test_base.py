import os
import sys
import unittest
import dlb

class TestBase(unittest.TestCase):

    SHM_KEY = ""

    @classmethod
    def setUpClass(cls):
        cls.SHM_KEY = "testsuite_%d" % os.getpid()

    @classmethod
    def tearDownClass(cls):
        shmem_exists = cls._test_and_delete_shmems()
        assert not shmem_exists, "Some shared memory files still existed!"

    @classmethod
    def _test_and_delete_shmems(cls):
        shmem_exists = False
        shmem_names = ["lewi", "lewi_async", "cpuinfo", "procinfo",
                       "async", "barrier", "talp", "test"]

        for name in shmem_names:
            shm_filename = "/dev/shm/DLB_%s_%s" % (name, cls.SHM_KEY)
            if os.path.exists(shm_filename):
                sys.stderr.write("ERROR: Shared memory %s still exists, removing...\n" % shm_filename)
                try:
                    os.unlink(shm_filename)
                except OSError as e:
                    sys.stderr.write("WARNING: Could not remove %s: %s\n" % (shm_filename, e))
                shmem_exists = True

        return shmem_exists
