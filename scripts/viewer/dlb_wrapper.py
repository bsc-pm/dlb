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

    def get_pid_list(self):
        array_size = self.get_num_cpus()
        pidlist = (ctypes.c_int*array_size)()
        nelems = ctypes.c_int()
        self.lib.DLB_Stats_GetPidList(ctypes.byref(pidlist), ctypes.byref(nelems), array_size)
        return [pidlist[i] for i in range(nelems.value)]

    def get_cpu_usage_list(self):
        array_size = self.get_num_cpus()
        usage_list = (ctypes.c_double*array_size)()
        nelems = ctypes.c_int()
        self.lib.DLB_Stats_GetCpuUsageList(ctypes.byref(usage_list), ctypes.byref(nelems), array_size)
        return [usage_list[i] for i in range(nelems.value)]

    def get_active_cpus_list(self):
        array_size = self.get_num_cpus()
        cpus_list = (ctypes.c_int*array_size)()
        nelems = ctypes.c_int()
        self.lib.DLB_Stats_GetActiveCpusList(ctypes.byref(cpus_list), ctypes.byref(nelems), array_size)
        return [cpus_list[i] for i in range(nelems.value)]
