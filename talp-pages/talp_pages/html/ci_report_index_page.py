from talp_pages.html.site import HTMLSite


class CIReportIndexPage(HTMLSite):
    def __init__(self, subsites) -> None:
        super().__init__("index.jinja", "index.html")
        self.sites = subsites

    def get_content(self):
        html = self.render_template(sites_dict=self.sites, zip=zip)
        return html
