import numpy as np
import plotly.graph_objects as go
from plotly.colors import sample_colorscale

from talp_pages.common import TALP_METRIC_TO_NAME_MAP, TALP_METRIC_TO_NESTING_MAP
from talp_pages.common import ExecutionMode
from talp_pages.analysis.scaling_analysis import ScalingAnalysisResult


class ScalingTableHtmlPlot:
    def __init__(self, result: ScalingAnalysisResult):
        self.result = result

    def _get_table(self):
        ressource_combinations = self.result.ressource_combinations
        df_datapoints = self.result.dataframe_list

        if ExecutionMode.HYBRID.value in self.result.execution_modes:
            eff_metrics = [
                "globalEfficiency",
                "parallelEfficiency",
                "mpiParallelEfficiency",
                "mpiCommunicationEfficiency",
                "mpiLoadBalance",
                "mpiLoadBalanceIn",
                "mpiLoadBalanceOut",
                "ompParallelEfficiency",
                "ompSchedulingEfficiency",
                "ompLoadBalance",
                "ompSerializationEfficiency",
                "computationScalability",
                "instructionScaling",
                "ipcScaling",
                "frequencyScaling",
            ]
            abs_metrics = ["IPC", "frequency", "elapsedTime"]

        elif ExecutionMode.MPI.value in self.result.execution_modes:
            eff_metrics = [
                "globalEfficiency",
                "parallelEfficiency",
                "mpiParallelEfficiency",
                "mpiCommunicationEfficiency",
                "mpiLoadBalance",
                "mpiLoadBalanceIn",
                "mpiLoadBalanceOut",
                "computationScalability",
                "instructionScaling",
                "ipcScaling",
                "frequencyScaling",
            ]
            abs_metrics = ["IPC", "frequency", "elapsedTime"]

        elif ExecutionMode.OPENMP.value in self.result.execution_modes:
            eff_metrics = [
                "globalEfficiency",
                "parallelEfficiency",
                "ompParallelEfficiency",
                "ompSchedulingEfficiency",
                "ompLoadBalance",
                "ompSerializationEfficiency",
                "computationScalability",
                "instructionScaling",
                "ipcScaling",
                "frequencyScaling",
            ]
            abs_metrics = ["IPC", "frequency", "elapsedTime"]

        elif ExecutionMode.SERIAL.value in self.result.execution_modes:
            eff_metrics = [
                "globalEfficiency",
                "computationScalability",
                "instructionScaling",
                "ipcScaling",
                "frequencyScaling",
            ]
            abs_metrics = ["IPC", "frequency", "elapsedTime"]

        metrics = eff_metrics + abs_metrics

        header = ["Metrics"]
        header.extend(
            [f"<b>{combination}</b>" for combination in ressource_combinations]
        )
        aligns = ["left"]
        aligns.extend(["center" for _ in metrics])
        values = [
            [
                f"{'-'*TALP_METRIC_TO_NESTING_MAP[metric]} {TALP_METRIC_TO_NAME_MAP[metric]}"
                for metric in metrics
            ]
        ]
        numeric_values = [
            datapoint[metrics].to_numpy().squeeze().round(2)
            for datapoint in df_datapoints
        ]
        values.extend(numeric_values)

        colors = [np.full_like(metrics, "rgb(250, 250, 250)").squeeze()]
        colors.extend(
            [
                sample_colorscale(
                    "rdylgn", np.clip(numeric_value, 0, 1), low=0.0, high=1.0
                )
                for numeric_value in numeric_values
            ]
        )
        # reset color for absolute metrics
        for col in colors[1:]:
            col[len(eff_metrics) : len(metrics)] = [
                "rgb(250, 250, 250)" for _ in abs_metrics
            ]

        fig = go.Figure(
            data=[
                go.Table(
                    header=dict(
                        values=header,
                        # line_color='white', fill_color='white',
                        align=aligns,
                        font=dict(color="black", size=20),
                    ),
                    cells=dict(
                        values=values,
                        line_color=colors,
                        fill_color=colors,
                        height=30,
                        align=aligns,
                        font=dict(color="black", size=16),
                    ),
                )
            ]
        )
        return fig

    def get_html(self) -> str:
        fig = self._get_table()
        fig.update_layout(
            autosize=True,
            height=650,
            margin=dict(l=50, r=50, b=0, t=0, pad=4),
            font=dict(
                family="Roboto Condensed, monospace",
                # size=18,
            ),
        )
        return fig.to_html(full_html=False)
