from python_test_base import TestBase
import unittest
import dlb

class TestPythonAPI(TestBase):

    def test(self):
        dlb.DLB_Init(0, "", "--drom=0 --barrier=no --shm-key=" + self.__class__.SHM_KEY)

        dlb.DLB_Disable()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Lend()
        dlb.DLB_Enable()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_SetMaxParallelism(1)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_UnsetMaxParallelism()
        dlb.DLB_SetObserverRole(1)

        # Lend
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Lend()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_LendCpu(0)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_LendCpus(1)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_LendCpuMask("")

        # Reclaim
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Reclaim()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_ReclaimCpu(0)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_ReclaimCpus(1)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_ReclaimCpuMask("")

        # Acquire
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_AcquireCpu(0)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_AcquireCpus(1)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_AcquireCpuMask("")
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_AcquireCpusInMask(1, "")

        # Borrow
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Borrow()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BorrowCpu(0)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BorrowCpus(1)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BorrowCpuMask("")
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BorrowCpusInMask(1, "")

        # Return
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Return()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_ReturnCpu(0)
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_ReturnCpuMask("")

        # Barrier
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_Barrier()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BarrierAttach()
        with self.assertRaises(dlb.DLBError):
            dlb.DLB_BarrierDetach()

        dlb.DLB_Finalize()

if __name__ == "__main__":
    unittest.main()
