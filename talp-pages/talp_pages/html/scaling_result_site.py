from talp_pages.html.site import HTMLChildPage
from talp_pages.analysis.scaling_analysis import ScalingAnalysisResult
from typing import List
from talp_pages.plots.scaling_table_html_plot import ScalingTableHtmlPlot

from operator import itemgetter
import logging


class ScalingResultSite(HTMLChildPage):
    def __init__(self, results: List[ScalingAnalysisResult]) -> None:
        super().__init__(
            template_name="scaling.jinja", file_name="scaling.html", icon="bi-graph-up"
        )
        self.results = results
        self.latest_change = max(
            [result.latest_change for result in self.results], key=itemgetter(1)
        )  # Get newest change

    def get_content(self):
        if not self.results:
            raise ValueError("Handed no results to ScalingResultSite")

        regions = [result.region for result in self.results]
        scaling_mode = self.results[0].scaling_mode  # just take the first

        configurations = {}
        regions_to_render = []

        for region_name, result in zip(regions, self.results):
            configurations[region_name] = []
            plot = ScalingTableHtmlPlot(result)
            try:
                figure = plot.get_html()
            except ValueError as e:
                logging.warning(
                    f"Trying to get region with name  {region_name} for scaling analysis but failed. Skipping that region... {e}"
                )

            for datapoint in result.dataframe_list:
                configurations[region_name].append(
                    {
                        "label": datapoint["ressourceLabel"].to_numpy().squeeze(),
                        "json_file": datapoint["jsonFile"].to_numpy().squeeze(),
                        "timestamp": datapoint["timestamp"].to_numpy().squeeze(),
                        "metadata": datapoint["metadata"].to_numpy().squeeze(),
                    }
                )
                scaling_mode = datapoint["scalingMode"].to_numpy().squeeze()

            if figure:
                regions_to_render.append({"name": region_name, "figure": figure})

        html = self.render_template(
            regions=regions_to_render,
            configurations=configurations,
            scaling_mode=scaling_mode,
        )
        return html
