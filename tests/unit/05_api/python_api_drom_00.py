from python_test_base import TestBase
import os
import unittest
import dlb

class TestPythonDROM(TestBase):

    def test(self):
        # Skip test on systems with less than 5 CPUs
        if dlb.DLB_DROM_GetNumCpus() < 5:
            return

        pid = os.getpid()

        dlb.DLB_Init(0, "", "--drom --shm-key=" + self.__class__.SHM_KEY)
        dlb.DLB_DROM_Attach()

        new_mask = "2,4"
        dlb.DLB_DROM_SetProcessMask(pid, new_mask, 0)
        mask = dlb.DLB_DROM_GetProcessMask(pid, 0)
        self.assertEqual(mask, new_mask, f"expected mask '{new_mask}', got '{mask}'")

        num_cpus = dlb.DLB_DROM_GetNumCpus()
        self.assertGreater(num_cpus, 0, f"expected at least 1 CPU, got {num_cpus}")

        dlb.DLB_DROM_Detach()
        dlb.DLB_Finalize()

if __name__ == "__main__":
    unittest.main()
