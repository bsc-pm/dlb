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
    run_folder = run_folders[0]  # Weak scaling
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
    run_folder = run_folders[1]  # Strong scaling
    print(run_folder)
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
