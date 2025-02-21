from typing import List, Tuple
import pandas as pd
import numpy as np
import logging
from enum import Enum
from talp_pages.io.run_folders import RunFolder
from talp_pages.io.dataframe_handling import ExecutionMode
from talp_pages.io.dataframe_handling import get_dfs
from dataclasses import dataclass
from datetime import datetime

# Slighlty hacky workaround, but works for now to disable ugly warning
pd.options.mode.chained_assignment = None  # default='warn'


@dataclass
class ScalingAnalysisArguments:
    run_folder: RunFolder


class ScalingMode(Enum):
    WEAK = "Weak Scaling"
    STRONG = "Strong Scaling"


@dataclass
class ScalingAnalysisResult:
    region: str
    scaling_mode: ScalingMode
    scaling_table: pd.DataFrame
    execution_modes: ExecutionMode
    latest_change: Tuple[datetime, str]  # Timestamp and git commit if available
    ressource_combinations: List[str]
    dataframe_list: List[pd.DataFrame]


class ScalingAnalysis:
    def __init__(self, arguments: ScalingAnalysisArguments) -> None:
        if not arguments:
            raise ValueError("No ScalingAnalysisArguments given")
        self.run_folder = arguments.run_folder
        # get df from run_folders
        self.df = get_dfs([self.run_folder], as_list=False)

    def __detect_scaling_mode(self, dfs: List[pd.DataFrame]) -> ScalingMode:
        # Assume a ordered list where the ressource with the lowest is the first
        # Use instructions to estimate the weak or strong scaling
        # Assumption: IFF Weak scaling the number of instructions/totalNumCpus should be roughly the same within a bound of +- 50%
        # Default mode. WEAK
        if not dfs:
            return ScalingMode.WEAK

        base_case = dfs[0]

        if base_case["instructions"].values[0] <= 0:
            logging.warning(
                f"Unable to detect ScalingMode, as no instructions are available. Run with PAPI. Assuming {ScalingMode.WEAK}"
            )
            return ScalingMode.WEAK

        # Assume Weak scaling -> if condition violated to strong scaling
        for df in dfs:
            instruction_growth = (
                df["instructions"].to_numpy() / base_case["instructions"].to_numpy()
            )
            cpu_growth = df["numCpus"].to_numpy() / base_case["numCpus"].to_numpy()

            if (instruction_growth / cpu_growth) < 0.75:
                logging.info(
                    f"ScalingMode found to be Strong as {instruction_growth / cpu_growth} < 0.75"
                )
                return ScalingMode.STRONG

        return ScalingMode.WEAK

    def __compute_scaling_metrics(
        self, region: str
    ) -> Tuple[pd.DataFrame, pd.DataFrame, List[pd.DataFrame]]:
        # Computes the scaling metrics and returns a tuple with the first beeing just the basic_anylsis like table
        # and the second one being a ordered list of the dataframes making up the BA table.

        clean_df = self.df.loc[self.df["regionName"] == region]
        if clean_df.empty:
            raise ValueError("Region not available")

        ressource_combinations = (
            clean_df.sort_values(by="totalNumCpus")["ressourceLabel"]
            .drop_duplicates()
            .to_numpy()
        )
        df_datapoints = []
        for combination in ressource_combinations:
            datapoint = (
                clean_df.loc[clean_df["ressourceLabel"] == combination]
                .sort_values(by=["timestamp"], ascending=False)
                .head(1)
            )
            df_datapoints.append(datapoint)
        if not df_datapoints:
            logging.error(
                f"Not able to compute the scaling metrics for dataframe: {clean_df}"
            )
            # return (pd.DataFrame(),[])

        scaling_mode = self.__detect_scaling_mode(df_datapoints)

        clean_df.loc[:, "scalingMode"] = scaling_mode.value

        reference_case = df_datapoints[0]

        if scaling_mode == ScalingMode.STRONG:
            # Assume instructions stay constant
            clean_df["instructionScaling"] = (
                reference_case["instructions"] / clean_df.loc[:, "instructions"]
            )
        if scaling_mode == ScalingMode.WEAK:
            clean_df["instructionScaling"] = (
                reference_case["instructions"] / reference_case["numCpus"]
            ) * (1 / (clean_df["instructions"] / clean_df["numCpus"]))

        clean_df.loc[:, "ipcScaling"] = clean_df.loc[:, "IPC"] / reference_case["IPC"]
        clean_df.loc[:, "frequencyScaling"] = (
            clean_df.loc[:, "frequency"] / reference_case["frequency"]
        )

        # NO Papi cases
        clean_df.loc[clean_df["instructionScaling"] == 0, "instructionScaling"] = np.nan
        clean_df.loc[clean_df["ipcScaling"] == 0, "ipcScaling"] = np.nan
        clean_df["instructionScaling"] = clean_df["instructionScaling"].fillna(np.inf)
        clean_df["ipcScaling"] = clean_df["ipcScaling"].fillna(np.inf)
        clean_df["frequencyScaling"] = clean_df["frequencyScaling"].fillna(np.inf)

        clean_df.loc[:, "computationScalability"] = (
            clean_df["instructionScaling"]
            * clean_df["ipcScaling"]
            * clean_df["frequencyScaling"]
        )
        clean_df.loc[:, "globalEfficiency"] = (
            clean_df.loc[:, "parallelEfficiency"]
            * clean_df.loc[:, "computationScalability"]
        )
        clean_df.loc[:, "speedup"] = (
            reference_case["elapsedTime"] / clean_df["elapsedTime"]
        )
        clean_df.loc[:, "globalEfficiencyBySpeedup"] = clean_df.loc[:, "speedup"] / (
            clean_df.loc[:, "numCpus"] / reference_case["numCpus"]
        )

        df_datapoints = []
        for combination in ressource_combinations:
            datapoint = (
                clean_df[clean_df["ressourceLabel"] == combination]
                .sort_values(by=["timestamp"], ascending=False)
                .head(1)
                .fillna(np.inf)
            )
            df_datapoints.append(datapoint)
        return (pd.DataFrame(), df_datapoints, scaling_mode, ressource_combinations)

    def get_results(self, regions: List[str]) -> List[ScalingAnalysisResult]:
        # Returns a result for every region
        results = []
        for region in regions:
            scaling_table, list_of_dfs, scaling_mode, ressource_combinations = (
                self.__compute_scaling_metrics(region)
            )
            execution_modes = [df["executionMode"].to_numpy() for df in list_of_dfs]
            latest_date = None
            latest_change = None
            for df in list_of_dfs:
                date = pd.to_datetime(df["timestamp"].to_numpy().squeeze())
                git_commit = str(df["gitCommitShort"].to_numpy().squeeze())

                try:
                    if date > latest_date:
                        latest_date = date
                        latest_change = (date, git_commit)
                except TypeError:
                    latest_change = (date, git_commit)

            results.append(
                ScalingAnalysisResult(
                    region=region,
                    scaling_mode=scaling_mode,
                    scaling_table=scaling_table,
                    execution_modes=execution_modes,
                    latest_change=latest_change,
                    ressource_combinations=ressource_combinations,
                    dataframe_list=list_of_dfs,
                )
            )

        return results
