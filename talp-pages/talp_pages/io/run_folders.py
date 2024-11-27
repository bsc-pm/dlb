from dataclasses import dataclass
from pathlib import Path
from typing import List
import os
from datetime import datetime
from talp_pages.common import TALP_JSON_TIMESTAMP_KEY
import logging
import json


@dataclass
class RunFolder:
    """Class for representing a Folder with a bunch of JSON files that are semantically connected through the folder strucutre"""

    relative_path: Path
    jsons: List[Path]
    latest_json: Path


def __set_newest_json(run_folder: RunFolder):
    newest_json = None
    newest_json_timestamp = None

    for json_entry in run_folder.jsons:
        with open(json_entry) as file:
            logging.debug(f"Opening {json_entry}")
            run_json = json.load(file)
            try:
                timestamp = datetime.fromisoformat(run_json[TALP_JSON_TIMESTAMP_KEY])
            except Exception:
                logging.critical(
                    f"Reading the {TALP_JSON_TIMESTAMP_KEY} key failed in {json_entry}. Falling back to the OS file creating date"
                )
                sys_time = os.path.getmtime(json_entry)
                timestamp = datetime.fromtimestamp(sys_time)

            if not newest_json_timestamp:  # base case
                newest_json_timestamp = timestamp
                newest_json = json_entry

            if timestamp.replace(tzinfo=None) > newest_json_timestamp.replace(
                tzinfo=None
            ):
                newest_json = json_entry
                newest_json_timestamp = timestamp

    run_folder.latest_json = newest_json


def get_run_folders(base_path: Path) -> List[RunFolder]:
    """Searches the base path and constructs the RunFolders it encouters, where at least one json file is contained"""
    run_folders = []

    for root, _, files in os.walk(base_path):
        json_files = [Path(root) / file for file in files if file.endswith(".json")]
        if json_files:
            relative_root = Path(root).relative_to(base_path)
            # dont add the most top level json
            if str(relative_root) != ".":
                run_folder = RunFolder(
                    relative_path=relative_root, jsons=json_files, latest_json=None
                )
                __set_newest_json(run_folder)
                run_folders.append(run_folder)
    return run_folders
