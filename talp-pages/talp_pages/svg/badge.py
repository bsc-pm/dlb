from urllib.request import urlopen, Request
from talp_pages.analysis.badge_analysis import BadgeResult


class Badge:
    def __init__(self, result: BadgeResult) -> None:
        self.result = result
        rc = result.ressource_configuration.replace(" ", "_")
        self.file_name = f"badge_{rc}.svg"

    def get_content(self):
        if not self.result.parallel_eff:
            raise ValueError("Empty result passed to Badge")
        parallel_efficiency = round(self.result.parallel_eff, 2)
        if self.result.parallel_eff < 0.6:
            bagde_url = f"https://img.shields.io/badge/Parallel_efficiency-{parallel_efficiency}-red"
        elif self.result.parallel_eff < 0.8:
            bagde_url = f"https://img.shields.io/badge/Parallel_efficiency-{parallel_efficiency}-orange"
        else:
            bagde_url = f"https://img.shields.io/badge/Parallel_efficiency-{parallel_efficiency}-green"

        return (
            urlopen(Request(url=bagde_url, headers={"User-Agent": "Mozilla"}))
            .read()
            .decode("utf-8")
        )
