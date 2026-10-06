from talp_pages.analysis.scaling_analysis import (
    ScalingAnalysis,
    ScalingAnalysisArguments,
    ScalingMode,
)
import pytest
from tests.helpers import get_relpath
from talp_pages.io.run_folders import get_run_folders
from talp_pages.io.dataframe_handling import ExecutionMode
from talp_pages.common import TALP_IMPLICIT_REGION_NAME


def get_run_folder_by_name(run_folders, name):
    return next(rf for rf in run_folders if rf.relative_path.name == name)


def test_invalid_creation():
    with pytest.raises(ValueError):
        _ = ScalingAnalysis(None)


def test_valid_creation():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = run_folders[0]
    args = ScalingAnalysisArguments(run_folder=run_folder)
    _ = ScalingAnalysis(args)


def test_get_results_weak():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = get_run_folder_by_name(run_folders, "weak-scaling")
    args = ScalingAnalysisArguments(run_folder)
    analysis = ScalingAnalysis(args)
    regions = [TALP_IMPLICIT_REGION_NAME]
    results = analysis.get_results(regions)
    assert len(results) == len(regions)
    result = results[0]  # only one because only one region
    assert result.region == TALP_IMPLICIT_REGION_NAME
    assert result.scaling_mode == ScalingMode.WEAK
    assert ExecutionMode.MPI.value in result.execution_modes
    assert ExecutionMode.HYBRID.value in result.execution_modes


def test_get_results_strong():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = get_run_folder_by_name(run_folders, "strong-scaling")
    args = ScalingAnalysisArguments(run_folder)
    analysis = ScalingAnalysis(args)
    regions = [TALP_IMPLICIT_REGION_NAME]
    results = analysis.get_results(regions)
    assert len(results) == len(regions)
    result = results[0]  # only one because only one region
    assert result.region == TALP_IMPLICIT_REGION_NAME
    assert result.scaling_mode == ScalingMode.STRONG
    assert ExecutionMode.MPI.value in result.execution_modes
    assert ExecutionMode.HYBRID.value in result.execution_modes


def test_get_results_strong_gpu_timezone_aware_timestamp():
    # Regression test: tz-aware "gitTimestamp" metadata used to make
    # df["timestamp"].to_numpy() fall back to an object-dtype array, which
    # pd.to_datetime() could not convert after squeeze().
    folder_path = get_relpath("jsons/run-folder-tz")
    run_folders = get_run_folders(folder_path)
    run_folder = get_run_folder_by_name(run_folders, "strong-scaling-gpu-tz")
    args = ScalingAnalysisArguments(run_folder)
    analysis = ScalingAnalysis(args)
    regions = [TALP_IMPLICIT_REGION_NAME]
    results = analysis.get_results(regions)
    result = results[0]
    assert result.latest_change is not None
    date, git_commit = result.latest_change
    assert date.tzinfo is not None
    assert git_commit == "abcdef0"
