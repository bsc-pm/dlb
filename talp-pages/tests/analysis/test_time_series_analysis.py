from talp_pages.analysis.time_series_analysis import (
    TimeSeriesAnalysis,
    TimeSeriesAnalysisArguments,
)
import pytest
from tests.helpers import get_relpath
from talp_pages.io.run_folders import get_run_folders
from talp_pages.io.dataframe_handling import get_ressource_combinations


def test_invalid_creation():
    with pytest.raises(ValueError):
        _ = TimeSeriesAnalysis(None)


def test_valid_creation():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = run_folders[0]
    args = TimeSeriesAnalysisArguments(run_folder=run_folder)
    _ = TimeSeriesAnalysis(args)


def test_get_results_weak():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = run_folders[0]  # Weak scaling
    args = TimeSeriesAnalysisArguments(run_folder)
    analysis = TimeSeriesAnalysis(args)
    ressource_combinations = get_ressource_combinations(run_folder)
    results = analysis.get_results(ressource_combinations)
    assert len(results) == len(ressource_combinations)
    assert (
        results[0].ressource_configuration == ressource_combinations[0]
    )  # Check if they are aligned
    assert (
        results[1].ressource_configuration == ressource_combinations[1]
    )  # Check if they are aligned
    assert not results[0].dataframe.empty
    assert not results[1].dataframe.empty
