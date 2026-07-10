from python_test_base import TestBase
import unittest
import dlb

class TestPythonTALP(TestBase):
    N = 100

    def test(self):
        dlb.DLB_Init(0, "", "--talp --shm-key=" + self.__class__.SHM_KEY)

        # Initialize global region with 1 measurement
        dlb_handle_1 = dlb.DLB_MonitoringRegionGetGlobal()
        with self.assertWarns(dlb.DLBWarning):
            dlb.DLB_MonitoringRegionStart(dlb_handle_1)
        dlb.DLB_MonitoringRegionStop(dlb_handle_1)
        self.assertEqual(dlb_handle_1.contents.num_measurements, 1)

        # Check that DLB_GLOBAL_REGION_NAME and DLB_GLOBAL_REGION identify the global region
        dlb_handle_with_name = dlb.DLB_MonitoringRegionRegister(dlb.DLB_GLOBAL_REGION_NAME)
        dlb.DLB_MonitoringRegionStart(dlb_handle_with_name)
        dlb.DLB_MonitoringRegionStop(dlb.DLB_GLOBAL_REGION)
        self.assertEqual(dlb_handle_1.contents.num_measurements, 2)

        # Register custom regions
        dlb_handle_1 = dlb.DLB_MonitoringRegionRegister("region 1")
        self.assertTrue(bool(dlb_handle_1))
        dlb_handle_2 = dlb.DLB_MonitoringRegionRegister("region 2")
        self.assertTrue(bool(dlb_handle_2))
        dlb_handle_3 = dlb.DLB_MonitoringRegionRegister(None)
        self.assertTrue(bool(dlb_handle_3))

        for _ in range(self.N):
            # Monitorized region
            dlb.DLB_MonitoringRegionStart(dlb_handle_1)

            # Nested monitoring region
            dlb.DLB_MonitoringRegionStart(dlb_handle_2)
            dlb.DLB_MonitoringRegionStop(dlb_handle_2)

            # Nested anonymous monitoring region
            dlb.DLB_MonitoringRegionStart(dlb_handle_3)
            dlb.DLB_MonitoringRegionStop(dlb.DLB_LAST_OPEN_REGION)
            with self.assertWarns(dlb.DLBWarning):
                dlb.DLB_MonitoringRegionStop(dlb_handle_3)

            # End monitoring region
            dlb.DLB_MonitoringRegionStop(dlb_handle_1)

        # Access monitor fields
        dlb_monitor = dlb_handle_1.contents

        print(dlb_monitor.name.decode())
        print(dlb_monitor.num_measurements)
        print(dlb_monitor.num_resets)
        print(dlb_monitor.elapsed_time)

        self.assertEqual(dlb_monitor.num_measurements, self.N)
        self.assertGreater(dlb_monitor.elapsed_time, 0)
        self.assertGreater(dlb_monitor.useful_time, 0)
        self.assertEqual(dlb_monitor.mpi_time, 0)
        self.assertEqual(dlb_monitor.omp_load_imbalance_time, 0)
        self.assertEqual(dlb_monitor.omp_scheduling_time, 0)
        self.assertEqual(dlb_monitor.omp_serialization_time, 0)

        # Reporting
        dlb.DLB_MonitoringRegionReport(dlb_handle_1)
        dlb.DLB_MonitoringRegionReport(dlb_handle_2)
        dlb.DLB_MonitoringRegionReport(dlb_handle_3)

        dlb.DLB_Finalize()

if __name__ == "__main__":
    unittest.main()
