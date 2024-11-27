from typing import List, Tuple
import pandas as pd
from talp_pages.io.run_folders import RunFolder
from talp_pages.io.dataframe_handling import get_dfs
from dataclasses import dataclass
from datetime import datetime


@dataclass
class TimeSeriesAnalysisArguments:
    run_folder: RunFolder


@dataclass
class TimeSeriesAnalysisResult:
    ressource_configuration: str
    dataframe: pd.DataFrame
    latest_change: Tuple[datetime, str]  # Timestamp and git commit if available


class TimeSeriesAnalysis:
    def __init__(self, arguments: TimeSeriesAnalysisArguments) -> None:
        if not arguments:
            raise ValueError("No arguments given to TimeSeriesAnalysis")
        self.run_folder = arguments.run_folder
        # get df from run_folders
        self.df = get_dfs([self.run_folder], as_list=False)  # we only have one

    def get_results(
        self, ressource_configurations: List[str]
    ) -> List[TimeSeriesAnalysisResult]:
        # Returns a result for every region
        results = []
        for ressource_configuration in ressource_configurations:
            dataframe = self.df[self.df["ressourceLabel"] == ressource_configuration]
            dataframe = dataframe.sort_values(
                by=["timestamp", "elapsedTime"], ascending=[False, False]
            )
            row_of_newest_timestamp = dataframe.loc[dataframe["timestamp"].idxmax()]
            date = pd.to_datetime(row_of_newest_timestamp["timestamp"])

            git_commit = str(row_of_newest_timestamp["gitCommitShort"])

            latest_change = (date, git_commit)

            results.append(
                TimeSeriesAnalysisResult(
                    ressource_configuration=ressource_configuration,
                    dataframe=dataframe,
                    latest_change=latest_change,
                )
            )

        return results
