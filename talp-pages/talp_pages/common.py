"""
File declaring some global scoped variables and functions
"""

from enum import Enum


class ExecutionMode(Enum):
    SERIAL = "Serial"
    OPENMP = "OpenMP"
    MPI = "MPI"
    HYBRID = "Hybrid MPI+OpenMP"


TALP_PAGES_VERSION = "3.5.1"
TALP_METRIC_TO_NAME_MAP = {
    "globalEfficiency": "Global Effiency",
    "globalEfficiencyBySpeedup": "Global Effiency from Speedup",
    "parallelEfficiency": "Parallel efficiency",
    "computationScalability": "Computation Scalability",
    "averageIPC": "Average IPC",
    "cycles": "Cycles",
    "instructions": "Useful Instructions",
    "IPC": "Useful IPC",
    "frequency": "Frequency [GHz]",
    "mpiParallelEfficiency": "MPI Parallel efficiency",
    "mpiCommunicationEfficiency": "MPI Communication efficiency",
    "mpiLoadBalance": "MPI Load balance",
    "mpiLoadBalanceIn": "MPI In-node load balance",
    "mpiLoadBalanceOut": "MPI Inter-node load balance",
    "ompParallelEfficiency": "OpenMP Parallel efficiency",
    "ompLoadBalance": "OpenMP Load balance",
    "ompSchedulingEfficiency": "OpenMP Scheduling efficiency",
    "ompSerializationEfficiency": "OpenMP Serialization efficiency",
    "instructionScaling": "Instructions scaling",
    "ipcScaling": "IPC scaling",
    "frequencyScaling": "Frequency scaling",
    "elapsedTime": "Elapsed time [s]",
    "speedup": "Speedup",
}

TALP_METRIC_TO_NESTING_MAP = {
    "globalEfficiency": 0,
    "globalEfficiencyBySpeedup": 0,
    "parallelEfficiency": 1,
    "computationScalability": 1,
    "averageIPC": 0,
    "cycles": 0,
    "instructions": 0,
    "IPC": 0,
    "frequency": 0,
    "mpiParallelEfficiency": 2,
    "mpiCommunicationEfficiency": 3,
    "mpiLoadBalance": 3,
    "mpiLoadBalanceIn": 4,
    "mpiLoadBalanceOut": 4,
    "ompParallelEfficiency": 2,
    "ompLoadBalance": 3,
    "ompSchedulingEfficiency": 3,
    "ompSerializationEfficiency": 3,
    "instructionScaling": 2,
    "ipcScaling": 2,
    "frequencyScaling": 2,
    "elapsedTime": 0,
    "speedup": 0,
}
# Some keys
TALP_IMPLICIT_REGION_NAME = "Global"
TALP_JSON_TIMESTAMP_KEY = "timestamp"
TALP_JSON_METADATA_KEY = "metadata"
TALP_VERSION_KEY = "dlbVersion"
TALP_JSON_POP_METRICS_KEY = "Application"
TALP_RESOURCES_KEY = "resources"
