#!/usr/bin/env python

import argparse
import logging
import requests

"""
    Script to automagically download the .zip archive of the last successful
    execution of that jobname on the branch: ref-name
    Requires python-gitlab to be installed

    Default behaviour: create empty zip file
"""


def add_arguments(parser: argparse.ArgumentParser):
    parser.description = (
        "Download artifacts from the latest successful GitHub job in that workflow"
    )
    parser.add_argument(
        "--project-name",
        help="Github project name (format: namespace/project)",
        required=True,
    )
    parser.add_argument(
        "--workflow-name",
        help="Name of the workflow to download the artifacts from",
        required=True,
    )
    parser.add_argument(
        "--artifact-name",
        help="Name of the artifact to download",
        required=True,
    )
    parser.add_argument(
        "--output-file", help="Output file name for downloaded artifacts", required=True
    )
    parser.add_argument("--ref-name", help="Github branch name", default="main")
    parser.add_argument(
        "--github-token",
        help="Personal GitHub access token with enough permissions",
    )
    parser.add_argument(
        "--log-level",
        help="Logging level (DEBUG, INFO, WARNING, ERROR)",
        default="INFO",
    )


def download_artifacts(headers, download_url, output_file=None):
    # Get artifacts for the job
    try:
        download_resp = requests.get(download_url, headers=headers)
        download_resp.raise_for_status()
        artifacts_file = download_resp.content
        logging.info("Artifacts downloaded successfully: %s", output_file)
    except Exception:
        logging.critical(
            "Was not able to download artifacts, continue with a empty zip directory"
        )
        # The binary string is just the bare minimum to have a valid empty zip file
        artifacts_file = b"PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    with open(output_file, "wb") as f:
        f.write(artifacts_file)


def get_workflow_id(headers, namespace, workflow_name):
    workflow_url = f"https://api.github.com/repos/{namespace}/actions/workflows"
    workflow_resp = requests.get(workflow_url, headers=headers)
    workflow_resp.raise_for_status()
    workflows = workflow_resp.json()["workflows"]
    workflow_id = next(
        (
            wf["id"]
            for wf in workflows
            if wf["name"] == workflow_name or wf["path"].endswith(workflow_name)
        ),
        None,
    )
    if not workflow_id:
        raise Exception(f'Workflow "{workflow_name}" not found.')

    return workflow_id


def get_latest_run_id(headers, namespace, workflow_id):
    runs_url = (
        f"https://api.github.com/repos/{namespace}/actions/workflows/{workflow_id}/runs"
    )
    runs_resp = requests.get(
        runs_url, headers=headers, params={"status": "completed", "per_page": 1}
    )
    runs_resp.raise_for_status()
    runs = runs_resp.json()["workflow_runs"]
    if not runs:
        raise ValueError("No successfully completed workflow runs found.")
    return runs[0]["id"]


def get_artifact_url(headers, namespace, latest_run_id, artifact_name):
    # Handle case when we could not find a latest run id
    if not latest_run_id:
        return ""
    artifacts_url = f"https://api.github.com/repos/{namespace}/actions/runs/{latest_run_id}/artifacts"
    artifacts_resp = requests.get(artifacts_url, headers=headers)
    artifacts_resp.raise_for_status()
    artifacts = artifacts_resp.json()["artifacts"]

    artifact = next((a for a in artifacts if a["name"] == artifact_name), None)
    if not artifact:
        raise ValueError(
            f'Artifact "{artifact_name}" not found in workflow run with id: {latest_run_id}'
        )
    return artifact["archive_download_url"]


def main(args):
    log_level = getattr(logging, args.log_level.upper(), None)
    if not isinstance(log_level, int):
        raise ValueError("Invalid log level: %s" % args.log_level)

    logging.basicConfig(level=log_level)

    # Get token
    http_request_headers = {
        "Authorization": f"token {args.github_token}",
        "Accept": "application/vnd.github+json",
    }
    # TODO add some sanity checks on project_name

    workflow_id = get_workflow_id(
        headers=http_request_headers,
        namespace=args.project_name,
        workflow_name=args.workflow_name,
    )
    try:
        latest_run_id = get_latest_run_id(
            headers=http_request_headers,
            workflow_id=workflow_id,
            namespace=args.project_name,
        )
    except ValueError as e:
        logging.critical("Unable to obtain the last run.")
        logging.critical(f"{e}")
        latest_run_id = ""

    try:
        download_url = get_artifact_url(
            headers=http_request_headers,
            namespace=args.project_name,
            latest_run_id=latest_run_id,
            artifact_name=args.artifact_name,
        )
    except ValueError as e:
        logging.critical("Could not get the download URL of the artifact.")
        logging.critical(f"{e}")
        download_url = ""

    download_artifacts(
        headers=http_request_headers,
        download_url=download_url,
        output_file=args.output_file,
    )
