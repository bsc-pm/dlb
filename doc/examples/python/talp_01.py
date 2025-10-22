#!/usr/bin/env python3
import time
import dlb

def print_monitor_info(monitor):
    m = monitor.contents
    name = m.name.decode()
    print(f"-- {name} --")
    print(f"  Num measurements: {m.num_measurements}")
    print(f"  Elapsed time:     {m.elapsed_time}")
    print()

if __name__ == "__main__":
    dlb.DLB_Init(0, "", "--talp")

    # Get handle to the global region
    global_monitor = dlb.DLB_MonitoringRegionGetGlobal()

    # Create multiple custom regions
    region_names = ["Region A", "Region B", "Region C"]
    regions = [dlb.DLB_MonitoringRegionRegister(name) for name in region_names]

    for i in range(5):
        print(f"\nIteration {i+1}: doing work...")

        dlb.DLB_MonitoringRegionStart(regions[0])
        time.sleep(0.1)
        dlb.DLB_MonitoringRegionStop(regions[0])

        dlb.DLB_MonitoringRegionStart(regions[1])
        time.sleep(0.3)
        dlb.DLB_MonitoringRegionStop(regions[1])

        dlb.DLB_MonitoringRegionStart(regions[2])
        time.sleep(0.2)
        dlb.DLB_MonitoringRegionStop(regions[2])

    # Stop global
    dlb.DLB_MonitoringRegionStop(global_monitor)

    # Print info for all monitors
    print("\n=== Monitoring results ===")
    print_monitor_info(global_monitor)
    for r in regions:
        print_monitor_info(r)

    dlb.DLB_Finalize()
