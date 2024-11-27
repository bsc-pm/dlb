#!/usr/bin/env python

import os
import argparse
import gitlab
import logging

"""
    Script to automagically choose a suitable authentication method and download the .zip archive of the last sucessfull 
    execution of that jobname on the branch: ref-name
    Requires python-gitlab to be installed

    Default behaviour: create empty zip file
"""


def add_arguments(parser: argparse.ArgumentParser):
    parser.description = "Download artifacts from the latest successful GitLab CI job"
    parser.add_argument(
        "--gitlab-url", help="GitLab URL (the thing before /api/*)", required=True
    )
    parser.add_argument(
        "--project-name",
        help="GitLab project name (format: namespace/project)",
        required=True,
    )
    parser.add_argument(
        "--job-name",
        help="Name of the GitLab CI job from which to download the artifact",
        required=True,
    )
    parser.add_argument(
        "--output-file", help="Output file name for downloaded artifacts", required=True
    )
    parser.add_argument("--ref-name", help="GitLab branch name", default="main")
    parser.add_argument(
        "--gitlab-token",
        help="Personal GitLab access token (if not specified it will try to use a job_token)",
    )
    parser.add_argument(
        "--log-level",
        help="Logging level (DEBUG, INFO, WARNING, ERROR)",
        default="INFO",
    )


def download_artifacts(
    gl, project_name, job_name, ref_name, gitlab_token=None, output_file=None
):
    try:
        project = gl.projects.get(project_name)
    except gitlab.exceptions.GitlabGetError as e:
        logging.error("Failed to get project details: %s", str(e))
        return

    logging.info("Found project ID: %s", project.id)

    # Get artifacts for the job
    try:
        artifacts_file = project.artifacts.download(
            ref_name=ref_name, job=job_name.strip()
        )
        logging.info("Artifacts downloaded successfully: %s", output_file)
    except Exception:
        logging.critical(
            "Wasnt able to download artifacts, continue with a empty zip directory"
        )
        # The binary string is just the bare minimum to have a valid empty zip file
        artifacts_file = b"PK\x05\x06\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"

    with open(output_file, "wb") as f:
        f.write(artifacts_file)


def main(args):
    log_level = getattr(logging, args.log_level.upper(), None)
    if not isinstance(log_level, int):
        raise ValueError("Invalid log level: %s" % args.log_level)

    logging.basicConfig(level=log_level)
    job_gitlab_token = None
    gitlab_token = None

    if args.gitlab_token:
        gitlab_token = args.gitlab_token
    else:
        job_gitlab_token = os.getenv("CI_JOB_TOKEN")
        logging.debug("Using the CI_JOB_TOKEN variant")

    gl = None
    if gitlab_token:
        gl = gitlab.Gitlab(args.gitlab_url, private_token=gitlab_token)
        gl.auth()
    elif job_gitlab_token:
        gl = gitlab.Gitlab(args.gitlab_url, job_token=job_gitlab_token)
        logging.debug("Using the CI_JOB_TOKEN variant")
    else:
        gl = gitlab.Gitlab(args.gitlab_url)

    download_artifacts(
        gl,
        args.project_name,
        args.job_name,
        args.ref_name,
        args.gitlab_token,
        args.output_file,
    )
