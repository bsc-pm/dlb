from talp_pages.html.site import HTMLChildPage
from talp_pages.analysis.time_series_analysis import TimeSeriesAnalysisResult
from typing import List
from talp_pages.plots.time_series_html_plot import TimeSeriesHtmlPlot


class TimeSeriesSite(HTMLChildPage):
    def __init__(self, result: TimeSeriesAnalysisResult, regions: List[str]) -> None:
        rc = result.ressource_configuration.replace(" ", "_")
        super().__init__(
            "time_series.jinja", file_name=f"evolution_{rc}.html", icon="bi-graph-up"
        )
        self.latest_change = result.latest_change
        self.result = result
        self.highlited_regions = regions

    def get_content(self):
        if not self.result:
            raise ValueError("Handed no results to TimeSeriesSites")
        plot = TimeSeriesHtmlPlot(result=self.result, regions=self.highlited_regions)
        html = self.render_template(figure=plot.get_html())
        return html
