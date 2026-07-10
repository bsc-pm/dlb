from pathlib import Path
import os
import sys
sys.path.insert(0, str(Path(__file__).parents[1]))
from conftest import run_binary, assert_approx, NS, TIME_TOLERANCE, METRIC_TOLERANCE
import pytest

BINARY = Path(__file__).parent / "test_omp_nested"

@pytest.mark.parametrize("repetitions", [1, 2])
def test_omp_states_profile(tmp_path, repetitions):
    output = run_binary(BINARY, tmp_path / "result.json",
                        n_proc=1, extra_env={
                            "OMP_NUM_THREADS": "2,2",
                            "OMP_PROC_BIND": "true",
                            "OMP_PLACES": "threads"},
                        args=[str(repetitions)])

    cpu_count = os.cpu_count() or 1

    # ----- Region: Global -----
    g = output["Application"]["Global"]

    # --- Structural ---
    assert g["numCpus"] == min(4, cpu_count)
    assert g["numMpiRanks"] == 0
    assert g["numOmpParallels"] == 4 * repetitions

    # --- Elapsed: serial(0.15) + parallels (0.5 + 0.5 + 0.10) ---
    elapsed_s = g["elapsedTime"] / NS
    assert_approx(elapsed_s, expected=1.25 * repetitions, label="elapsedTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Useful time: all threads' compute accumulated ---
    # serial: 0.25s * 1 thread + parallels: (0.5+0.25+0.5+0.5)s
    useful_s = g["usefulTime"] / NS
    assert_approx(useful_s, expected=2.0 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompSerializationTime: 3 idle threads * 0.25s serial section
    #                           + 2 threads * 0.5s during first parallel
    #                           + 1 thread  * 0.5s during second parallel ---
    serial_s = g["ompSerializationTime"] / NS
    assert_approx(serial_s, expected=2.25 * repetitions, label="ompSerializationTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompLoadImbalanceTime: (0.25+0.5)s from unequal parallel work ---
    imbalance_s = g["ompLoadImbalanceTime"] / NS
    assert_approx(imbalance_s, expected=0.75 * repetitions, label="ompLoadImbalanceTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompSchedulingTime: small but nonzero from dynamic schedule ---
    sched_s = g["ompSchedulingTime"] / NS
    assert sched_s > 0.0, f"ompSchedulingTime should be > 0, got {sched_s:.4f}s"

    # --- Efficiency checks ---
    assert_approx(g["ompParallelEfficiency"], expected=0.40, label="ompParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompLoadBalance"], expected=0.85, label="ompLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSerializationEfficiency"], expected=0.47, label="ompSerializationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSchedulingEfficiency"], expected=1.00, label="ompSchedulingEfficiency",
                  absolute_tol=METRIC_TOLERANCE)


    # ----- Region: Small -----
    s = output["Application"]["Small"]

    # --- Structural ---
    assert s["numCpus"] == 1
    assert s["numMpiRanks"] == 0
    assert s["numOmpParallels"] == 1 * repetitions

    # --- Useful time:  0.10s * 1 thread ---
    useful_s = s["usefulTime"] / NS
    assert_approx(useful_s, expected=0.1 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompSerializationTime: 3 idle threads * 0.1s ---
    serial_s = s["ompSerializationTime"] / NS
    assert_approx(serial_s, expected=0.3 * repetitions, label="ompSerializationTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Efficiency checks ---
    assert_approx(s["ompParallelEfficiency"], expected=0.25, label="ompParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(s["ompLoadBalance"], expected=1.00, label="ompLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(s["ompSerializationEfficiency"], expected=0.25, label="ompSerializationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(s["ompSchedulingEfficiency"], expected=1.00, label="ompSchedulingEfficiency",
                  absolute_tol=METRIC_TOLERANCE)


    # ----- Region: Thread -----
    t = output["Application"]["Thread"]

    # --- Structural ---
    assert t["numCpus"] == min(3, cpu_count)
    assert t["numMeasurements"] == 4 * repetitions
    assert t["numMpiRanks"] == 0
    assert t["numOmpParallels"] == 0


    # --- Useful time:  0.5s + 0.25s + 0.5s + 0.5s = 1.75s ---
    useful_s = t["usefulTime"] / NS
    assert_approx(useful_s, expected=1.75 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Efficiency checks ---
    assert_approx(t["parallelEfficiency"], expected=1.00, label="parallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
