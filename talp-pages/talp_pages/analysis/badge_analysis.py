from talp_pages.io.run_folders import RunFolder
from talp_pages.io.dataframe_handling import get_dfs
from dataclasses import dataclass
from typing import List

import logging


@dataclass
class BadgeResult:
    parallel_eff: float
    ressource_configuration: str


class BadgeAnalysis:
    def __init__(self, run_folder: RunFolder, region_for_badge: str) -> None:
        if not run_folder or not region_for_badge:
            raise ValueError("Badge Analyis cannot be constructed")
        self.run_folder = run_folder
        # get df from run_folders
        self.region = region_for_badge
        self.df = get_dfs([self.run_folder], as_list=False)

    def get_results(self, ressource_configurations: List[str]) -> List[BadgeResult]:
        results = []
        for rc in ressource_configurations:
            ressource_label_matching = self.df["ressourceLabel"] == rc
            region_matching = self.df["regionName"] == self.region

            target_df = self.df.loc[ressource_label_matching & region_matching]
            if not target_df.empty:
                parallel_efficiency = (
                    target_df.sort_values(by=["timestamp"], ascending=False)
                    .loc[:, "parallelEfficiency"]
                    .head(1)
                    .values[0]
                )
                results.append(
                    BadgeResult(
                        parallel_eff=parallel_efficiency, ressource_configuration=rc
                    )
                )
            else:
                logging.error(
                    f"Unable to find region {self.region} in {rc} for {self.df['runFolder'].head(1)}. Continuing with erroneous badge."
                )
                results.append(BadgeResult(parallel_eff=0, ressource_configuration=rc))

        return results
