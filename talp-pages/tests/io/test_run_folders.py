from talp_pages.io.run_folders import get_run_folders
from tests.helpers import get_relpath


def test_get_dfs():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    assert run_folders[0].latest_json is not None
    assert run_folders[1].latest_json is not None


def test_get_dfs_no_timestamp():
    folder_path = get_relpath("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    assert run_folders[0].latest_json is not None
    assert run_folders[1].latest_json is not None


def test_get_run_folders_empty_from_bottom_most_dir():
    folder_path = get_relpath("jsons/run-folder/strong-scaling")
    run_folders = get_run_folders(folder_path)
    assert len(run_folders) == 0
