from typing import List, Union
import pandas as pd
from pathlib import Path
import json
from datetime import datetime
import numpy as np
from talp_pages.io.run_folders import RunFolder
from talp_pages.common import (
    TALP_JSON_TIMESTAMP_KEY,
    TALP_JSON_METADATA_KEY,
    ExecutionMode,
    TALP_VERSION_KEY,
    TALP_IMPLICIT_REGION_NAME,
    TALP_JSON_POP_METRICS_KEY,
    TALP_RESOURCES_KEY,
)

import logging


def detect_execution_mode(df) -> ExecutionMode:
    # First check if for all its 1
    total_threads = df["totalNumCpus"] - df["totalNumMpiRanks"]
    if df["totalNumCpus"].sum() == len(df["totalNumCpus"]):
        # Only 1 totalNumCpu per entry -> all entries are serial
        return ExecutionMode.SERIAL
    elif df["totalNumMpiRanks"].sum() == 0:
        # Not all runs are serial, but no MPI Ranks:
        return ExecutionMode.OPENMP
    elif total_threads.sum() == 0:
        # We have MPI Ranks, but no additional CPUs #TODO Think of LeWI
        return ExecutionMode.MPI
    else:
        return ExecutionMode.HYBRID


def __load_talp_json_3_5(json_input, json_path):
    # we do this a bit ugly for now and reorganize the json a bit:
    for region in json_input[TALP_JSON_POP_METRICS_KEY].keys():
        json_input[TALP_JSON_POP_METRICS_KEY][region]["regionName"] = region

    df = pd.DataFrame(json_input[TALP_JSON_POP_METRICS_KEY].values())
    try:
        df["runTimestamp"] = pd.to_datetime(json_input[TALP_JSON_TIMESTAMP_KEY])
    except KeyError:
        df["runTimestamp"] = pd.to_datetime(datetime.now())
        logging.critical(
            f"Could not read {TALP_JSON_TIMESTAMP_KEY} key in {json_path}. Using current time instead"
        )
    try:
        metadata = json_input[TALP_JSON_METADATA_KEY]
    except KeyError:
        metadata = {}
    df["metadata"] = json.dumps(metadata)

    if metadata:
        df["gitCommitShort"] = metadata["gitCommit"][:7]  # Git Short shas have 7 chars
        try:
            df["gitTimestamp"] = pd.to_datetime(metadata["gitTimestamp"])
            df["timestamp"] = pd.to_datetime(metadata["gitTimestamp"])
        except KeyError:
            df["timestamp"] = df["runTimestamp"]
    else:
        df["timestamp"] = df["runTimestamp"]
        df["gitCommitShort"] = ""

    try:
        resources = json_input[TALP_RESOURCES_KEY]
    except KeyError:
        resources = {"numCpus": 0, "numMpiRanks": 0}

    num_cpus = resources["numCpus"]
    num_mpi_ranks = resources["numMpiRanks"]
    df["totalNumCpus"] = num_cpus
    df["totalNumMpiRanks"] = num_mpi_ranks

    df["dlbVersion"] = "3.5"
    df["jsonFile"] = str(json_path)
    df["implicitRegionName"] = TALP_IMPLICIT_REGION_NAME

    # Compute some additional data

    invalid_cycles = df["cycles"] < 0
    invalid_instructions = df["instructions"] < 0
    # CLAMP cycles and instructions
    df.loc[invalid_cycles, "cycles"] = 0
    df.loc[invalid_instructions, "instructions"] = 0
    df["IPC"] = df["instructions"] / df["cycles"]
    df["IPC"] = df["IPC"].fillna(np.inf)

    if (sum(invalid_cycles) > 0) or sum(invalid_instructions) > 0:
        logging.warning(
            f"Invalid PAPI Counter values observed in {json_path}. Please check PAPI and DLB installations"
        )

    df["frequency"] = df["cycles"] / df["usefulTime"]
    # Normalize to seconds
    df["elapsedTime"] = df["elapsedTime"] * 1e-9

    execution_mode = detect_execution_mode(df)
    if execution_mode == ExecutionMode.HYBRID:
        # Then we do the X
        df["totalThreads"] = (
            df.loc[:, "totalNumCpus"] / df.loc[:, "totalNumMpiRanks"]
        ).astype(int)
        df["ressourceLabel"] = (
            df.loc[:, "totalNumMpiRanks"].astype(str)
            + "xMPI "
            + df.loc[:, "totalThreads"].astype(str)
            + "xOpenMP"
        )
    elif execution_mode == ExecutionMode.SERIAL:
        df["ressourceLabel"] = ExecutionMode.SERIAL.value
    else:
        df["ressourceLabel"] = (
            df.loc[:, "totalNumCpus"].astype(str) + f"x{execution_mode.value}"
        )

    df["executionMode"] = execution_mode.value
    # finally sort
    df = df.sort_values(by=["elapsedTime"], ascending=False)
    return df


def load_talp_json_df(json_path: Path) -> pd.DataFrame:
    with open(json_path) as file:
        run_json = json.load(file)
        df = None
        # First check version
        if run_json[TALP_VERSION_KEY].startswith("3.5"):
            df = __load_talp_json_3_5(run_json, json_path)
        else:
            raise ValueError(
                f"Unable to find parser for {run_json[TALP_VERSION_KEY]}. Please use a compatible TALP-Pages release for DLB version {run_json[TALP_VERSION_KEY]}"
            )
        return df


def get_dfs(
    run_folders: List[RunFolder], as_list=True
) -> Union[List[pd.DataFrame], pd.DataFrame]:
    dfs_outer = []
    for run_folder in run_folders:
        dfs_inner = []
        for json_entry in run_folder.jsons:
            df = load_talp_json_df(json_entry)
            df["runFolder"] = str(run_folder.relative_path)
            dfs_inner.append(df)

        # Now combine the inner semantically clustered jsons and add to list
        df_of_run_folder = pd.concat(dfs_inner)
        dfs_outer.append(df_of_run_folder)

    if as_list:
        return dfs_outer
    else:
        return pd.concat(dfs_outer)


def get_ressource_combinations(run_folder: RunFolder):
    df = get_dfs([run_folder], as_list=True)[0]  # we only have one
    return (
        df.sort_values(by="totalNumCpus")["ressourceLabel"].drop_duplicates().to_numpy()
    )
