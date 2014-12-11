import ctypes
import os.path

class DLB_Stats():
    def __init__(self, dlb_path="/usr"):
        dlb_lib = dlb_path + "/lib/libdlb.so"
        if not os.path.isfile(dlb_lib):
            raise Exception("DLB library cannot be found at " + dlb_lib)
        self.lib = ctypes.cdll.LoadLibrary(dlb_lib)
        self.lib.DLB_Stats_GetCpuUsage.restype = ctypes.c_double
        self.lib.DLB_Stats_Init()

    def __del__(self):
        self.lib.DLB_Stats_Finalize()

    def get_num_cpus(self):
        return self.lib.DLB_Stats_GetNumCpus()

    def get_cpu_usage(self, pid):
        return self.lib.DLB_Stats_GetCpuUsage(pid)

    def get_active_cpus(self, pid):
        return self.lib.DLB_Stats_GetActiveCpus(pid)

    def get_load_avg(self, pid):
        load = (ctypes.c_double*3)()
        self.lib.DLB_Stats_GetLoadAvg(pid, ctypes.byref(load))
        return [load[i] for i in range(len(load))]
