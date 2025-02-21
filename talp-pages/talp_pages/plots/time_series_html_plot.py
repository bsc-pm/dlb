from plotly.subplots import make_subplots
import plotly.graph_objects as go
from typing import List
import numpy as np
from talp_pages.io.dataframe_handling import ExecutionMode
from talp_pages.analysis.time_series_analysis import TimeSeriesAnalysisResult

from talp_pages.common import TALP_IMPLICIT_REGION_NAME, TALP_METRIC_TO_NAME_MAP


class TimeSeriesHtmlPlot:
    def __init__(self, result: TimeSeriesAnalysisResult, regions: List[str]):
        self.result = result
        self.df = result.dataframe
        self.highlighted_regions = regions

    def __get_colors(self, regions: List[str]):
        region_to_colors = {}
        for region in regions:
            # compute average % of region
            mean_region = self.df.loc[
                self.df["regionName"] == region, "elapsedTime"
            ].mean()
            mean_implicit = self.df.loc[
                self.df["regionName"] == TALP_IMPLICIT_REGION_NAME, "elapsedTime"
            ].mean()
            percentage = mean_region / mean_implicit
            alpha = np.single(np.sqrt(percentage) + 0.2)
            alpha = np.clip(alpha, 0, 1)
            region_to_colors[region] = (
                f"rgba({np.random.randint(0,255)}, {np.random.randint(0,255)}, {np.random.randint(0,255)}, {alpha.round(2)})"
            )
        return region_to_colors

    def __get_scatter(self, region, metric, color, show_legend=False):
        if region in self.highlighted_regions:
            is_visible = True
        else:
            is_visible = "legendonly"

        return go.Scatter(
            x=self.df[self.df["regionName"] == region]["timestamp"],
            y=self.df[self.df["regionName"] == region][metric],
            text=self.df[self.df["regionName"] == region]["metadata"],
            mode="lines+markers",
            name=region,
            legendgroup=region,
            showlegend=show_legend,
            marker_color=color,
            line=dict(color=color),
            visible=is_visible,
            # hovertemplate='%{text}'),
        )

    def _get_figure(self):
        regions = self.df["regionName"].unique().tolist()
        if ExecutionMode.HYBRID.value in self.df["executionMode"].to_numpy():
            metrics = [
                "elapsedTime",
                "IPC",
                "instructions",
                "frequency",
                "parallelEfficiency",
                "mpiParallelEfficiency",
                "mpiCommunicationEfficiency",
                "mpiLoadBalance",
                "ompParallelEfficiency",
                "ompSchedulingEfficiency",
                "ompLoadBalance",
                "ompSerializationEfficiency",
            ]
            specs = [
                [{"colspan": 6}, None, None, None, None, None],  # elapsedTime
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'IPC','instructions','frequency'
                [{"colspan": 6}, None, None, None, None, None],  # parallelEfficiency
                [{"colspan": 6}, None, None, None, None, None],  # mpiParallelEfficiency
                [
                    {"colspan": 3},
                    None,
                    None,
                    {"colspan": 3},
                    None,
                    None,
                ],  # 'mpiCommunicationEfficiency','mpiCommunicationEfficiency'
                [{"colspan": 6}, None, None, None, None, None],  # ompParallelEfficiency
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'ompSchedulingEfficiency','ompLoadBalance','ompSerializationEfficiency'
            ]
            execution_mode = ExecutionMode.HYBRID

        elif ExecutionMode.MPI.value in self.df["executionMode"].to_numpy():
            metrics = [
                "elapsedTime",
                "IPC",
                "instructions",
                "frequency",
                "parallelEfficiency",
                "mpiParallelEfficiency",
                "mpiCommunicationEfficiency",
                "mpiLoadBalance",
            ]
            specs = [
                [{"colspan": 6}, None, None, None, None, None],  # elapsedTime
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'IPC','instructions','frequency'
                [{"colspan": 6}, None, None, None, None, None],  # parallelEfficiency
                [{"colspan": 6}, None, None, None, None, None],  # mpiParallelEfficiency
                [
                    {"colspan": 3},
                    None,
                    None,
                    {"colspan": 3},
                    None,
                    None,
                ],  # 'mpiCommunicationEfficiency','mpiCommunicationEfficiency'
            ]
            execution_mode = ExecutionMode.MPI

        elif ExecutionMode.OPENMP.value in self.df["executionMode"].to_numpy():
            metrics = [
                "elapsedTime",
                "IPC",
                "instructions",
                "frequency",
                "parallelEfficiency",
                "ompParallelEfficiency",
                "ompSchedulingEfficiency",
                "ompLoadBalance",
                "ompSerializationEfficiency",
            ]
            specs = [
                [{"colspan": 6}, None, None, None, None, None],  # elapsedTime
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'IPC','instructions','frequency'
                [{"colspan": 6}, None, None, None, None, None],  # parallelEfficiency
                [{"colspan": 6}, None, None, None, None, None],  # ompParallelEfficiency
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'ompSchedulingEfficiency','ompLoadBalance','ompSerializationEfficiency'
            ]
            execution_mode = ExecutionMode.OPENMP

        else:
            # Serial
            metrics = [
                "elapsedTime",
                "IPC",
                "instructions",
                "frequency",
            ]
            specs = [
                [{"colspan": 6}, None, None, None, None, None],  # elapsedTime
                [
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                    {"colspan": 2},
                    None,
                ],  # 'IPC','instructions','frequency'
            ]
            execution_mode = ExecutionMode.SERIAL

        region_to_colors = self.__get_colors(regions)
        # sampled_colors = list(sample_colorscale(colorscale, len(regions)))

        # Create Big figure
        fig = make_subplots(
            rows=len(specs),
            cols=6,
            specs=specs,
            horizontal_spacing=0.07,
            vertical_spacing=0.05,
            subplot_titles=[TALP_METRIC_TO_NAME_MAP[metric] for metric in metrics],
            print_grid=False,
        )

        # Elapsed time
        metric = "elapsedTime"
        plots_done = 1
        for region in regions:
            fig.add_trace(
                self.__get_scatter(region, metric, region_to_colors[region], True),
                col=1,
                row=1,
            )
        fig["layout"]["yaxis"]["title"] = "Time in [s]"

        # IPC
        metric = "IPC"
        plots_done += 1
        for region in regions:
            fig.add_trace(
                self.__get_scatter(region, metric, region_to_colors[region]),
                col=1,
                row=2,
            )
        fig["layout"][f"yaxis{plots_done}"]["title"] = "IPC"

        # instructions
        metric = "instructions"
        plots_done += 1
        for region in regions:
            fig.add_trace(
                self.__get_scatter(region, metric, region_to_colors[region]),
                col=3,
                row=2,
            )
        fig["layout"][f"yaxis{plots_done}"]["title"] = "# instructions"

        # frequency
        metric = "frequency"
        plots_done += 1
        for region in regions:
            fig.add_trace(
                self.__get_scatter(region, metric, region_to_colors[region]),
                col=5,
                row=2,
            )
        fig["layout"][f"yaxis{plots_done}"]["title"] = "GHz"

        if (
            execution_mode == ExecutionMode.HYBRID
            or execution_mode == ExecutionMode.MPI
            or execution_mode == ExecutionMode.OPENMP
        ):
            # Parallel Effiency
            metric = "parallelEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=1,
                    row=3,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

        if (
            execution_mode == ExecutionMode.HYBRID
            or execution_mode == ExecutionMode.MPI
        ):
            # communicationEfficiency
            metric = "mpiParallelEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=1,
                    row=4,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

            metric = "mpiCommunicationEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=1,
                    row=5,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

            metric = "mpiLoadBalance"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=4,
                    row=5,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

        if (
            execution_mode == ExecutionMode.OPENMP
            or execution_mode == ExecutionMode.HYBRID
        ):
            #'ompSchedulingEfficiency','ompLoadBalance','ompSerializationEfficiency',
            metric = "ompParallelEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=1,
                    row=6 if execution_mode == ExecutionMode.HYBRID else 4,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

            metric = "ompSchedulingEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=1,
                    row=7 if execution_mode == ExecutionMode.HYBRID else 5,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

            metric = "ompLoadBalance"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=3,
                    row=7 if execution_mode == ExecutionMode.HYBRID else 5,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

            metric = "ompSerializationEfficiency"
            plots_done += 1
            for region in regions:
                fig.add_trace(
                    self.__get_scatter(region, metric, region_to_colors[region]),
                    col=5,
                    row=7 if execution_mode == ExecutionMode.HYBRID else 5,
                )
            fig["layout"][f"yaxis{plots_done}"]["title"] = "Efficiency [0-1]"
            fig.update_layout(**{f"yaxis{plots_done}": dict(range=[0, 1])})

        fig.update_traces(mode="markers+lines")
        return fig

    def get_html(self) -> str:
        fig = self._get_figure()
        fig.update_layout(
            title="Performance metrics evolution",
            legend_title="Regions",
            autosize=True,
            height=2000,
            margin=dict(l=50, r=50, b=100, t=100, pad=4),
            font=dict(
                family="Roboto Condensed, monospace",
                # size=18,
            ),
        )
        return fig.to_html(full_html=False)
