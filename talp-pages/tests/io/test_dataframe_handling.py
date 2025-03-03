import pytest
from talp_pages.io.dataframe_handling import load_talp_json_df, get_dfs
from talp_pages.io.run_folders import get_run_folders
from tests.helpers import get_json_path
import json
import pandas as pd


JSON_LOAD_FIELDS = [
    "runTimestamp",
    "totalNumCpus",
    "totalNumMpiRanks",
    "dlbVersion",
    "jsonFile",
    "implicitRegionName",
    "totalThreads",
    "ressourceLabel",
    "executionMode",
]


def test_handle_wrong_dlb_version():
    json_path = get_json_path("jsons/unit-tests/wrong_dlb_version.json")

    with pytest.raises(ValueError):
        _ = load_talp_json_df(json_path)


def test_handle_version_3_5_1():
    json_path = get_json_path("jsons/unit-tests/dlb_version_351.json")

    df = load_talp_json_df(json_path)
    assert df is not None


def test_handle_new_dlb_version():
    json_path = get_json_path("jsons/unit-tests/new_dlb_version.json")

    df = load_talp_json_df(json_path)
    assert df is not None


def test_handle_no_runtime_stamp():
    json_path = get_json_path("jsons/unit-tests/no_timestamp.json")

    df = load_talp_json_df(json_path)
    df["now"] = pd.Timestamp("now")
    # less than one second ago -> because we should use the current timestamp
    assert (df["now"] - df["runTimestamp"]).dt.total_seconds().squeeze() < 1


def test_handle_no_ressources():
    json_path = get_json_path("jsons/unit-tests/no_ressources.json")

    df = load_talp_json_df(json_path)
    assert df["totalNumCpus"].squeeze() == 0
    assert df["totalNumMpiRanks"].squeeze() == 0


def test_ressources_loading():
    json_path = get_json_path("jsons/unit-tests/correct_2MPI.json")
    df = load_talp_json_df(json_path)
    assert df["totalNumCpus"].to_numpy() == 2
    assert df["totalNumMpiRanks"].to_numpy() == 2


def test_metadata_empty_loading():
    json_path = get_json_path("jsons/unit-tests/correct_2MPI.json")
    df = load_talp_json_df(json_path)
    assert df["metadata"].squeeze() == json.dumps({})


def test_metadata_non_empty_loading():
    json_path = get_json_path("jsons/unit-tests/correct_2MPI_with_metadata.json")
    df = load_talp_json_df(json_path)
    assert df["metadata"].squeeze() == json.dumps(
        {
            "gitCommit": "<somehash>",
            "gitBranch": "branch",
            "defaultGitBranch": "main",
            "isMergeRequest": True,
            "gitTimestamp": "2024-09-02T14:11:24",
        }
    )


def test_load_talp_json_df():
    json_path = get_json_path("jsons/unit-tests/correct_2MPI.json")
    df = load_talp_json_df(json_path)
    assert df["runTimestamp"].squeeze() == pd.to_datetime("2024-09-02T14:11:24")
    assert df["dlbVersion"].squeeze() == "3.5"
    assert df["executionMode"].squeeze() == "MPI"


def test_load_talp_json_df_serial():
    json_path = get_json_path("jsons/unit-tests/complete_serial.json")
    df = load_talp_json_df(json_path)
    assert df["runTimestamp"].squeeze() == pd.to_datetime("2024-09-02T14:11:24")
    assert df["dlbVersion"].squeeze() == "3.5"
    assert df["executionMode"].squeeze() == "Serial"


def test_get_dfs():
    folder_path = get_json_path("jsons/run-folder")
    run_folders = get_run_folders(folder_path)
    dfs = get_dfs(run_folders, as_list=True)
    assert len(dfs) == 2

    df = get_dfs(run_folders, as_list=False)
    assert isinstance(df, pd.DataFrame)
