import json, subprocess, os
from pathlib import Path
import pytest

DLB_HOME = os.environ.get("DLB_HOME")
TIME_TOLERANCE = 0.10   # 10% relative tolerance
METRIC_TOLERANCE = 0.02 # 2% absolute tolerance
NS = 1_000_000_000

def run_binary(binary: Path, output_json: Path, n_proc: int = 1,
               extra_env: dict = None, args: list = None):
    if DLB_HOME is None:
        raise TypeError

    env_pairs = [
        f"LD_PRELOAD={DLB_HOME}/lib/libdlb_mpi.so",
        f"DLB_ARGS=--talp --talp-output-file={output_json}",
    ]
    if extra_env:
        env_pairs += [f"{k}={v}" for k, v in extra_env.items()]

    if n_proc == 1:
        cmd = ["env", *env_pairs, str(binary)]
    else:
        cmd = ["mpirun", "-n", str(n_proc), "env", *env_pairs, str(binary)]

    if args:
        cmd += args

    subprocess.run(cmd, check=True)
    return json.loads(output_json.read_text())

def assert_approx(value, expected, label, *, relative_tol=None, absolute_tol=None):
    if (relative_tol is None) == (absolute_tol is None):
        raise ValueError("Provide exactly one of relative_tol or absolute_tol")

    if relative_tol is not None:
        lo, hi = expected * (1 - relative_tol), expected * (1 + relative_tol)
        tol_str = f"±{int(relative_tol*100)}%"
    else:
        lo, hi = expected - absolute_tol, expected + absolute_tol
        tol_str = f"±{absolute_tol:.3f}"

    assert lo <= value <= hi, (
        f"{label}: expected {expected:.3f} {tol_str}, got {value:.3f}"
    )
