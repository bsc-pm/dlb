#!/usr/bin/env python3
import dlb
import os

if __name__ == "__main__":
    pid = os.getpid()
    print(f"Starting DROM example. PID: {pid}")
    dlb.DLB_Init(0, "", "--drom")

    new_mask = "2,4"
    dlb.DLB_DROM_SetProcessMask(pid, new_mask, 0)
    mask = dlb.DLB_DROM_GetProcessMask(pid, 0)
    print(f"New mask: {mask}")

    new_mask = "1-4"
    dlb.DLB_DROM_SetProcessMask(pid, new_mask, 0)
    mask = dlb.DLB_DROM_GetProcessMask(pid, 0)
    print(f"New mask: {mask}")

    dlb.DLB_Finalize()
