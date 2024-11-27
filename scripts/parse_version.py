#!/usr/bin/env python

import argparse
import re


def version_to_hexa(major, minor, patch):
    version_number = (major << 16) + (minor << 8) + patch
    print(hex(version_number))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('type', choices=['major', 'minor', 'patch', 'hexa'])
    parser.add_argument('version')
    args = parser.parse_args()

    version_regex = r"([0-9]+)[\.-]([0-9]+)[\.-]([0-9]+).*"
    match = re.match(version_regex, args.version)
    try:
        major = int(match.group(1))
        minor = int(match.group(2))
        patch = int(match.group(3))
    except Exception:
        major = 0
        minor = 0
        patch = 0

    if args.type == 'major':
        print(major)
    elif args.type == 'minor':
        print(minor)
    elif args.type == 'patch':
        print(patch)
    elif args.type == 'hexa':
        return version_to_hexa(major, minor, patch)


if __name__ == '__main__':
   main()
