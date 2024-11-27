from talp_pages.common import TALP_PAGES_VERSION
import pathlib
from jinja2 import Environment, FileSystemLoader
from datetime import datetime

TALP_PAGES_TEMPLATE_PATH = pathlib.Path(__file__).parent.joinpath("templates").resolve()


class HTMLSite:
    def __init__(self, template_name: str, file_name: str) -> None:
        self.page_metadata = {
            "page_metadata": {
                "timestamp_string": datetime.now().isoformat(timespec="seconds"),
                "version": TALP_PAGES_VERSION,
                "home": "../index.html",
            }
        }
        self.jinja_env = Environment(loader=FileSystemLoader(TALP_PAGES_TEMPLATE_PATH))
        self.template_name = template_name
        self.template = self.jinja_env.get_template(self.template_name)
        self.file_name = file_name

    def render_template(self, **context) -> str:
        # Render the template with the provided context
        return self.template.render({**context, **self.page_metadata})

    def get_content(self) -> str:
        raise NotImplementedError("Subclass needs to overrride this method")


class HTMLChildPage(HTMLSite):
    def __init__(self, template_name: str, file_name: str, icon: str) -> None:
        super().__init__(template_name, file_name)
        self.latest_change = None
        self.icon = icon
