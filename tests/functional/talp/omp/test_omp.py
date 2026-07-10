from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parents[1]))
from conftest import run_binary, assert_approx, NS, TIME_TOLERANCE, METRIC_TOLERANCE
import pytest

BINARY = Path(__file__).parent / "test_omp"

@pytest.mark.parametrize("repetitions", [1, 2])
def test_omp_states_profile(tmp_path, repetitions):
    output = run_binary(BINARY, tmp_path / "result.json",
                        n_proc=1, extra_env={"OMP_NUM_THREADS": "4"}, args=[str(repetitions)])
    g = output["Application"]["Global"]

    # --- Structural ---
    assert g["numMpiRanks"] == 0
    assert g["numOmpParallels"] == 1 * repetitions

    # --- Elapsed: serial(1.0) + parallel dominated by thread0(1.0) + loop(~0.125s) ---
    elapsed_s = g["elapsedTime"] / NS
    assert_approx(elapsed_s, expected=2.1 * repetitions, label="elapsedTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Useful time: all threads' compute accumulated ---
    # serial: 1.0s * 1 thread + parallel: (1.0+0.75+0.5+0.25)s + loop work
    useful_s = g["usefulTime"] / NS
    assert_approx(useful_s, expected=4.0 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompSerializationTime: 3 idle threads * 1.0s serial section ---
    serial_s = g["ompSerializationTime"] / NS
    assert_approx(serial_s, expected=3.0 * repetitions, label="ompSerializationTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompLoadImbalanceTime: (0+0.25+0.5+0.75)s from unequal parallel work ---
    imbalance_s = g["ompLoadImbalanceTime"] / NS
    assert_approx(imbalance_s, expected=1.5 * repetitions, label="ompLoadImbalanceTime",
                  relative_tol=TIME_TOLERANCE)

    # --- ompSchedulingTime: small but nonzero from dynamic schedule ---
    sched_s = g["ompSchedulingTime"] / NS
    assert sched_s > 0.0, f"ompSchedulingTime should be > 0, got {sched_s:.4f}s"

    # --- Efficiency checks ---
    assert_approx(g["ompParallelEfficiency"], expected=0.48, label="ompParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompLoadBalance"], expected=0.83, label="ompLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSerializationEfficiency"], expected=0.58, label="ompSerializationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSchedulingEfficiency"], expected=0.99, label="ompSchedulingEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
