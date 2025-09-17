#!/usr/bin/env python

import argparse

import talp_pages.cli.ci_report as ci_report
import talp_pages.cli.metadata as metadata
import talp_pages.cli.download_gitlab as download_gitlab
import talp_pages.cli.download_github as download_github
from talp_pages.common import TALP_PAGES_VERSION


def main():
    parser = argparse.ArgumentParser(
        prog="talp", description="TALP-Pages CLI interface."
    )

    parser.add_argument("--version", action="version", version=TALP_PAGES_VERSION)
    subparsers = parser.add_subparsers(title="features", help="", dest="features")

    ci_report_parser = subparsers.add_parser(
        "ci-report",
        help="Generate a HTML report containing performance evolution and insight plots.",
    )
    metadata_parser = subparsers.add_parser("metadata", help="Add metdata to JSONS")
    download_gitlab_parser = subparsers.add_parser(
        "download-gitlab", help="Download GitLab Artifacts"
    )
    download_github_parser = subparsers.add_parser(
        "download-github", help="Download GitHub Artifacts"
    )

    ci_report.add_arguments(ci_report_parser)
    metadata.add_arguments(metadata_parser)
    download_gitlab.add_arguments(download_gitlab_parser)
    download_github.add_arguments(download_github_parser)

    args = parser.parse_args()

    print(f"Welcome to TALP-Pages ({TALP_PAGES_VERSION})")

    if args.features == "ci-report":
        ci_report.main(args)
    elif args.features == "metadata":
        metadata.main(args)
    elif args.features == "download-gitlab":
        download_gitlab.main(args)
    elif args.features == "download-github":
        download_github.main(args)
    else:
        parser.print_help()


if __name__ == "__main__":
    main()
