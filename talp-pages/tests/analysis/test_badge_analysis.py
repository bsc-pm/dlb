from talp_pages.analysis.badge_analysis import BadgeAnalysis
import pytest
from tests.helpers import get_relpath
from talp_pages.io.run_folders import get_run_folders
from talp_pages.io.dataframe_handling import get_ressource_combinations
from talp_pages.common import TALP_IMPLICIT_REGION_NAME


def test_invalid_creation():
    with pytest.raises(ValueError):
        _ = BadgeAnalysis(None, None)

    with pytest.raises(ValueError):
        _ = BadgeAnalysis(1, None)

    with pytest.raises(ValueError):
        _ = BadgeAnalysis(None, 1)


def test_valid_creation():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = run_folders[0]
    _ = BadgeAnalysis(run_folder, "test")


def test_get_results():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    run_folder = run_folders[0]
    ba = BadgeAnalysis(run_folder, TALP_IMPLICIT_REGION_NAME)
    ressource_combinations = get_ressource_combinations(run_folder)
    results = ba.get_results(ressource_combinations)
    assert len(results) == len(ressource_combinations)
