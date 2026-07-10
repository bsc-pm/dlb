from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parents[1]))
from conftest import run_binary, assert_approx, NS, TIME_TOLERANCE, METRIC_TOLERANCE
import pytest

BINARY = Path(__file__).parent / "test_hybrid"

@pytest.mark.parametrize("repetitions", [1, 2])
def test_hybrid_profile(tmp_path, repetitions):
    output = run_binary(BINARY, tmp_path / "result.json",
                        n_proc=2, extra_env={"OMP_NUM_THREADS": "2"}, args=[str(repetitions)])
    g = output["Application"]["Test"]

    # --- Structural ---
    assert output["resources"]["numMpiRanks"] == 2
    assert g["numMpiRanks"] == 2
    assert g["numOmpParallels"] == 1 * 2 * repetitions

    # --- Elapsed: serial(0.5) + parallel rank0(1.0) (mpi(~0.5) is overlapped) ≈ 1.5s ---
    elapsed_s = g["elapsedTime"] / NS
    assert_approx(elapsed_s, expected=1.5 * repetitions, label="elapsedTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Useful time (accumulated, all ranks * all threads) ---
    # rank0: serial(0.5*1) + parallel(1.0+0.5) = 2.0s
    # rank1: serial(0.5*1) + parallel(0.5+0.25) = 1.25s
    # total ≈ 3.25s
    useful_s = g["usefulTime"] / NS
    assert_approx(useful_s, expected=3.25 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- MPI time: rank1 waits ~0.5s, rank0 ~0s → total ~0.5s ---
    mpi_s = g["mpiTime"] / NS
    assert_approx(mpi_s, expected=0.5 * repetitions, label="mpiTime",
                  relative_tol=TIME_TOLERANCE)

    # --- MPI worker idle time: rank1 waits ~0.5s, rank0 ~0s → total ~0.5s ---
    mpi_worker_s = g["mpiWorkerIdleTime"] / NS
    assert_approx(mpi_worker_s, expected=0.5 * repetitions, label="mpiTime",
                  relative_tol=TIME_TOLERANCE)

    # --- OMP serialization---
    # rank0: 1 worker thread idle during 0.5s serial
    # rank1: 1 worker thread idle during 0.5s serial + 0.5s after parallel
    serial_s = g["ompSerializationTime"] / NS
    assert_approx(serial_s, expected=1.5 * repetitions, label="ompSerializationTime",
                  relative_tol=TIME_TOLERANCE)

    # --- OMP load imbalance: rank0(0.5s) + rank1(0.25s) = 0.75s ---
    imbalance_s = g["ompLoadImbalanceTime"] / NS
    assert_approx(imbalance_s, expected=0.75 * repetitions, label="ompLoadImbalanceTime",
                  relative_tol=TIME_TOLERANCE)

    # --- OMP scheduling time: small but nonzero from dynamic schedule ---
    sched_s = g["ompSchedulingTime"] / NS
    assert sched_s > 0.0, f"ompSchedulingTime should be > 0, got {sched_s:.4f}s"

    # --- Efficiency checks ---
    assert_approx(g["parallelEfficiency"], expected=0.54, label="parallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["mpiParallelEfficiency"], expected=0.83, label="mpiParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["mpiCommunicationEfficiency"], expected=1.00, label="mpiCommunicationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["mpiLoadBalance"], expected=0.83, label="mpiLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompParallelEfficiency"], expected=0.625, label="ompParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompLoadBalance"], expected=0.875, label="ompLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSerializationEfficiency"], expected=0.71, label="ompSerializationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["ompSchedulingEfficiency"], expected=0.99, label="ompSchedulingEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
