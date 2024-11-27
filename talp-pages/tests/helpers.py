import os
from pathlib import Path


def get_json_path(file):
    return os.path.join(Path(__file__).parent, Path(file))


def get_relpath(file):
    return os.path.join(Path(__file__).parent, Path(file))
