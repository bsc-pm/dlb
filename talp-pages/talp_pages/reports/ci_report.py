from typing import List
from pathlib import Path
from dataclasses import dataclass
from operator import itemgetter
from tqdm import tqdm

from talp_pages.io.run_folders import RunFolder
from talp_pages.analysis.scaling_analysis import (
    ScalingAnalysis,
    ScalingAnalysisArguments,
)
from talp_pages.analysis.time_series_analysis import (
    TimeSeriesAnalysis,
    TimeSeriesAnalysisArguments,
)
from talp_pages.analysis.badge_analysis import BadgeAnalysis
from talp_pages.io.dataframe_handling import get_ressource_combinations
from talp_pages.html.scaling_result_site import ScalingResultSite
from talp_pages.html.time_series_result_site import TimeSeriesSite
from talp_pages.html.ci_report_index_page import CIReportIndexPage
from talp_pages.svg.badge import Badge
from talp_pages.io.output import write_badge, write_site


@dataclass
class CIReportArgs:
    run_folders: List[RunFolder]
    selected_regions: List[str]
    region_for_badge: str
    with_badge: bool


class CIReport:
    def __init__(self, args: CIReportArgs) -> None:
        self.run_folders = args.run_folders
        self.selected_regions = args.selected_regions
        self.region_for_badge = args.region_for_badge
        self.with_badge = args.with_badge

    def write_to_folder(self, output_path: Path):
        sites = {}
        # for each run folder get the sites
        for run_folder in tqdm(self.run_folders, desc="Processing subfolders"):
            # first get scaling result site one per region
            scaling_results = ScalingAnalysis(
                ScalingAnalysisArguments(run_folder=run_folder)
            ).get_results(self.selected_regions)
            scaling_result_site = ScalingResultSite(scaling_results)
            # one entry per ressource combination
            ressource_configurations = get_ressource_combinations(run_folder)
            time_series_results = TimeSeriesAnalysis(
                TimeSeriesAnalysisArguments(run_folder=run_folder)
            ).get_results(ressource_configurations)

            time_series_sites = []
            for rc, result in zip(ressource_configurations, time_series_results):
                time_series_sites.append(TimeSeriesSite(result, self.selected_regions))

            badges = []
            if self.with_badge:
                bagde_results = BadgeAnalysis(
                    run_folder=run_folder, region_for_badge=self.region_for_badge
                ).get_results(ressource_configurations=ressource_configurations)
                for rc, result in zip(ressource_configurations, bagde_results):
                    badges.append(Badge(result))

            sites[run_folder.relative_path] = {
                "scaling_result_site": scaling_result_site,
                "ressource_configurations": ressource_configurations,
                "badges": badges,
                "region_for_badge": self.region_for_badge,
                "time_series_sites": time_series_sites,
                "relative_path": run_folder.relative_path,
                "latest_change": max(
                    [
                        site.latest_change
                        for site in [scaling_result_site, *time_series_sites]
                    ],
                    key=itemgetter(1),
                ),  # Get newest across subsites
            }
            # Write out the things
            subfolder = output_path.joinpath(run_folder.relative_path)
            for site in [scaling_result_site, *time_series_sites]:
                write_site(subfolder, site)
            for badge in badges:
                write_badge(subfolder, badge)

        index_page = CIReportIndexPage(subsites=sites)
        write_site(output_path, index_page)
