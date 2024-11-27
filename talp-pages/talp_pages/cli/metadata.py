#!/usr/bin/env python

import argparse
import os
import logging
from dataclasses import dataclass
from typing import Union
from talp_pages.io.run_folders import get_run_folders
import json
from datetime import datetime


def add_arguments(parser: argparse.ArgumentParser):
    # Adding argument for JSON file
    parser.add_argument(
        "-i",
        "--input-path",
        dest="path",
        help="Root path to search the *.json files in",
        required=True,
    )
    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        dest="log_level",
        help="Logger Level",
        default="WARNING",
    )
    parser.add_argument(
        "--git-commit", dest="git_commit", help="Git commit to put into the metadata"
    )
    parser.add_argument(
        "--git-branch", dest="git_branch", help="Git branch to put into the metadata"
    )
    parser.add_argument(
        "--git-timestamp",
        dest="git_timestamp",
        help="Timestamp (in ISO 8601 format) of git commit to put into the metadata",
    )
    parser.add_argument(
        "--is-merge-request",
        action=argparse.BooleanOptionalAction,
        dest="is_merge_request",
        help="Git branch to put into the metadata",
    )


def _verify_input(args):
    path = None

    # Check if the JSON file exists
    if os.path.exists(args.path):
        found_a_json = False
        for _, _, filenames in os.walk(args.path):
            for _ in [f for f in filenames if f.endswith(".json")]:
                found_a_json = True

        if not found_a_json:
            logging.error(
                f"The specified path '{args.path}' doesnt contain any .json files "
            )
            raise Exception("Path empty of .json files")
    else:
        logging.error(f"The specified path '{args.path}' does not exist.")
        raise Exception("Not existing path")

    path = args.path

    return path


@dataclass
class Metadata:
    git_commit: str
    git_branch: str
    git_timestamp: datetime
    default_git_branch: str
    is_merge_request: bool

    def dict(self) -> dict:
        return {
            "gitCommit": self.git_commit,
            "gitBranch": self.git_branch,
            "gitTimestamp": self.git_timestamp.replace(tzinfo=None).isoformat(),
            "defaultGitBranch": self.default_git_branch,
            "isMergeRequest": self.is_merge_request,
        }


UNKNOWN_METADATA = Metadata(
    "unkown commit", "unkown branch", None, "unkown default branch", False
)


def parse_date(date_string: str) -> datetime:
    # workaround until python 3.11 where datetime support reading timeszoned strings
    if date_string.endswith("Z"):
        date_string = date_string[:-1]
    dt = datetime.fromisoformat(date_string)
    return dt


def get_metadata(args: Union[argparse.Namespace, None]) -> Metadata:
    metadata = UNKNOWN_METADATA
    set_from_cli = False

    if args:
        # first try if its specified in the args, then dont use autodetect
        if args.git_commit:
            metadata.git_commit = f"{args.git_commit}"
            set_from_cli = True

        if args.git_branch:
            metadata.git_branch = args.git_branch
            set_from_cli = True

        if args.is_merge_request:
            metadata.is_merge_request = args.is_merge_request
            set_from_cli = True

    if set_from_cli:
        logging.info(
            "User specified command line arguments, so we dont autodetect the metadata"
        )
        return metadata
    # gitlab Merge request
    if "CI_MERGE_REQUEST_ID" in os.environ:
        metadata.git_commit = os.environ.get(
            "CI_COMMIT_SHA", UNKNOWN_METADATA.git_commit
        )
        metadata.git_branch = os.environ.get(
            "CI_MERGE_REQUEST_SOURCE_BRANCH_NAME", UNKNOWN_METADATA.git_branch
        )
        metadata.git_timestamp = parse_date(
            os.environ.get("CI_COMMIT_TIMESTAMP", UNKNOWN_METADATA.git_timestamp)
        )
        metadata.default_git_branch = os.environ.get(
            "CI_DEFAULT_BRANCH", UNKNOWN_METADATA.default_git_branch
        )
        metadata.is_merge_request = True
        logging.info("Auto detected Metadata from GITLAB Merge Request")
        return metadata

    # gitlab normal pipeline
    if "CI_COMMIT_SHA" in os.environ:
        metadata.git_commit = os.environ.get(
            "CI_COMMIT_SHA", UNKNOWN_METADATA.git_commit
        )
        metadata.git_branch = os.environ.get(
            "CI_COMMIT_BRANCH", UNKNOWN_METADATA.git_branch
        )
        metadata.git_timestamp = parse_date(
            os.environ.get("CI_COMMIT_TIMESTAMP", UNKNOWN_METADATA.git_timestamp)
        )
        metadata.is_merge_request = False
        logging.info("Auto detected Metadata from GITLAB normal pipeline")
        return metadata

    # Github Pull request
    if "GITHUB_HEAD_REF" in os.environ:
        metadata.git_commit = os.environ.get("GITHUB_SHA", UNKNOWN_METADATA.git_commit)
        metadata.git_branch = os.environ.get(
            "GITHUB_HEAD_REF", UNKNOWN_METADATA.git_branch
        )
        metadata.default_git_branch = "GitHub doenst expose the default branch via ENV"
        metadata.is_merge_request = True
        logging.info("Auto detected Metadata from Github Pull Request")
        return metadata

    # Github Normal action
    if "GITHUB_REF_NAME" in os.environ:
        metadata.git_commit = os.environ.get("GITHUB_SHA", UNKNOWN_METADATA.git_commit)
        metadata.git_branch = os.environ.get(
            "GITHUB_REF_NAME", UNKNOWN_METADATA.git_branch
        )
        metadata.default_git_branch = "GitHub doenst expose the default branch via ENV"
        metadata.is_merge_request = False
        logging.info("Auto detected Metadata from Github normal Action")
        return metadata

    logging.error(
        "Couldnt determine the metadata. Please use the command line interface to specify them."
    )
    return metadata


def add_metadata(metadata: Metadata, talp_json) -> None:
    metadata_dict = metadata.dict()

    if "metadata" in talp_json.keys():
        for key in metadata_dict.keys():
            if key in talp_json["metadata"].keys():
                logging.error(f"metadata.{key} already present in JSON")
                raise Exception(f"metadata.{key} already present in JSON")

        logging.info("Adding new metadata to existing metadata")
        already_present_metadata = talp_json["metadata"]
        talp_json["metadata"] = {**already_present_metadata, **metadata_dict}
    else:
        talp_json["metadata"] = metadata_dict


def main(parsed_args):
    path = _verify_input(parsed_args)

    log_level = getattr(logging, parsed_args.log_level.upper(), None)
    logging.basicConfig(level=log_level)

    # First detect the folder structure and append to the runs
    run_folders = get_run_folders(path)

    metadata = get_metadata(parsed_args)

    for run_folder in run_folders:
        for json_file_path in run_folder.jsons:
            # Open all the jsons but do in-place replacement of content
            talp_json = None
            with open(json_file_path, "r") as file:
                logging.debug(f"Start processing file {json_file_path}")
                talp_json = json.load(file)
                add_metadata(metadata, talp_json)
            with open(json_file_path, "w") as file:
                file.write(json.dumps(talp_json, indent=4))
                logging.debug(f"Done processing file {json_file_path}")
