from python_test_base import TestBase
import unittest
import dlb
from ctypes import POINTER

class TestPythonBarrier(TestBase):

    def test(self):
        dlb.DLB_Init(0, "0-5", "--barrier --shm-key=" + self.__class__.SHM_KEY)

        barrier1 = dlb.DLB_BarrierNamedRegister("barrier 1", dlb.DLB_BARRIER_LEWI_OFF)
        self.assertTrue(bool(barrier1), "barrier1 should be valid")

        barrier2 = dlb.DLB_BarrierNamedRegister("barrier 2", dlb.DLB_BARRIER_LEWI_ON)
        self.assertTrue(bool(barrier2), "barrier2 should be valid")

        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BarrierNamedAttach(barrier2)

        dlb.DLB_BarrierNamed(barrier2)

        dlb.DLB_PrintShmem(0, dlb.DLB_COLOR_AUTO)

        dlb.DLB_BarrierNamedDetach(barrier1)

        with self.assertWarns(dlb.DLBWarning):
            dlb.DLB_BarrierNamed(barrier1)

        dlb.DLB_PrintShmem(0, dlb.DLB_COLOR_AUTO)

        dlb.DLB_Finalize()

if __name__ == "__main__":
    unittest.main()
