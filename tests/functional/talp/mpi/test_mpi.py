from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).parents[1]))
from conftest import run_binary, assert_approx, NS, TIME_TOLERANCE, METRIC_TOLERANCE
import pytest

BINARY = Path(__file__).parent / "test_mpi"

@pytest.mark.parametrize("repetitions", [1, 2])
def test_mpi_sendrecv_profile(tmp_path, repetitions):
    output = run_binary(BINARY, tmp_path / "result.json", n_proc=2, args=[str(repetitions)])
    g = output["Application"]["Global"]

    # --- Structural checks ---
    assert output["resources"]["numCpus"] == 2
    assert output["resources"]["numMpiRanks"] == 2
    assert g["numMpiRanks"] == 2
    assert g["numMpiCalls"] == (3 + 2 * repetitions) * 2

    # --- Elapsed time: should be ~2.0s ---
    elapsed_s = g["elapsedTime"] / NS
    assert_approx(elapsed_s, expected=2.0 * repetitions, label="elapsedTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Useful (CPU) time: ~2.5s accumulated across both ranks ---
    useful_s = g["usefulTime"] / NS
    assert_approx(useful_s, expected=2.5 * repetitions, label="usefulTime",
                  relative_tol=TIME_TOLERANCE)

    # --- MPI time: ~1.5s accumulated ---
    mpi_s = g["mpiTime"] / NS
    assert_approx(mpi_s, expected=1.5 * repetitions, label="mpiTime",
                  relative_tol=TIME_TOLERANCE)

    # --- Efficiency sanity checks ---
    assert_approx(g["mpiParallelEfficiency"], expected=0.625, label="mpiParallelEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["mpiCommunicationEfficiency"], expected=0.75, label="mpiCommunicationEfficiency",
                  absolute_tol=METRIC_TOLERANCE)
    assert_approx(g["mpiLoadBalance"], expected=0.83, label="mpiLoadBalance",
                  absolute_tol=METRIC_TOLERANCE)
