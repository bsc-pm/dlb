from pathlib import Path
import os

import logging

from talp_pages.html.site import HTMLSite
from talp_pages.svg.badge import Badge


def write_badge(output_path: Path, badge: Badge):
    os.makedirs(output_path, exist_ok=True)
    file_path = os.path.join(output_path, badge.file_name)
    logging.debug(f"Writing Badge {file_path}")
    with open(file_path, "w") as file:
        file.write(badge.get_content())


def write_site(output_path: Path, site: HTMLSite):
    os.makedirs(output_path, exist_ok=True)
    file_path = os.path.join(output_path, site.file_name)
    logging.debug(f"Writing Site {file_path}")
    with open(file_path, "w") as file:
        file.write(site.get_content())
