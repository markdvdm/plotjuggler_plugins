"""
@file mkversion.py
@brief Generate a version header file with relevant info
@copyright Copyright 2022,2023 Elroy Air Inc. All rights reserved.
@author Jon Reeves <jon.reeves@elroyair.com>
"""
import datetime
import getpass  # for username
import json
import os
import re
import socket  # for hostname
import subprocess
from argparse import ArgumentParser
from contextlib import suppress
from pathlib import Path


def get_commit():
    """
    @brief Get the hash of the current HEAD commit
    @return The git hash as an ascii hex string
    """
    with suppress(Exception):
        for line in (
            subprocess.check_output("git rev-parse HEAD", shell=True)
            .decode()
            .splitlines()
        ):
            line = line.strip()
            if line:
                return line
    return ""


def get_repo_root():
    """
    @brief Get the absolute path of the git repo root
    @return The path string
    """
    with suppress(Exception):
        for line in (
            subprocess.check_output("git rev-parse --show-toplevel", shell=True)
            .decode()
            .splitlines()
        ):
            line = line.strip()
            if line:
                return line
    return ""


def get_repo_dirty_status():
    """
    @brief Check if a repo has uncommitted changes staged or unstaged
    @return True if there are uncommitted changes, False otherwise
    """
    res = False
    try:
        subprocess.check_output("git diff-index --quiet HEAD --", shell=True)
    except Exception:
        res = True
    return res


def get_semantic_version(version_str):
    """
    @param[in] version_str Optional version string. If no version string is
                           given, we will look for a VERSION file at the
                           top level
    @return A 4-tuple of version info: major, minor, patch, build

    If build is missing from the version string that comes from either the
    command line or the version file, it will be set to 0
    """
    if version_str is None:
        root_dir = get_repo_root()
        if not root_dir:
            raise ValueError("Git root path is empty. Is this a valid repo?")
        version_path = os.path.join(root_dir, "VERSION")
        with open(version_path, "r") as version_file:
            version_str = version_file.read().strip()

    version_pieces = version_str.split(".")
    if len(version_pieces) != 3:
        raise ValueError(
            "Version must be a semantic version string with optional build (e.g. 1.0.0-3)"
        )

    major = version_pieces[0]
    minor = version_pieces[1]
    patch = version_pieces[2]
    build = 0

    # look for build suffix
    build_split = version_pieces[2].split("-")
    if len(build_split) == 2:
        patch = build_split[0]
        build = build_split[1]

    return (major, minor, patch, build)


def get_host():
    """
    @return The hostname of the machine running this script
    """
    return socket.gethostname()


def get_user():
    """
    @return The username of who is running this script
    """
    return getpass.getuser()


def write_c_header(info_dict, path):
    """
    @brief Serialize version info in `info_dict` to a C header file

    Schema for info_dict is as follows:
        {
            'semantic_version': The semantic version tuple with exactly 4 sub-components
            'dirty_status': The git repo dirty status
            'timestamp': Timestamp of the running of this script
            'commit_str': The git commit hash as an ascii hex string
            'commit_bin_arr': The git commit hash as an array of bytes
            'host': The hostname of the machine running this script
            'user': The username of who is running this script
        }
    """

    file_name = os.path.splitext(os.path.basename(path))[0]
    # scrub the file name to make the header guard:
    guard_str = re.sub(r"[^.a-zA-Z0-9_]", "_", file_name)

    contents = (
        "// auto-generated file: do not edit by hand\n"
        "// generation date/time: {0}\n"
        "//\n"
        "// NOTE: mkversion will use the same macros for versions for any version file\n"
        "// generated with the tool. Do not use this file as a global include, instead\n"
        "// create a source module which declares namespace-protected variables and/or\n"
        "// accessor methods and initialize the return values with the macros declared\n"
        "// here\n"
        "#ifndef __{1}_H__\n"
        "#define __{1}_H__\n"
        "#define MKVERSION_SEM_VER_MAJOR        {2}\n"
        "#define MKVERSION_SEM_VER_MINOR        {3}\n"
        "#define MKVERSION_SEM_VER_PATCH        {4}\n"
        "#define MKVERSION_SEM_VER_BUILD        {5}\n"
        '#define MKVERSION_GIT_SHA_STR          "{6}"\n'
        "#define MKVERSION_GIT_SHA_BIN_ARR      {{ {7} }}\n"
        "#define MKVERSION_GIT_SHA_BIN_ARR_LEN  {8}\n"
        '#define MKVERSION_BUILD_HOST           "{9}"\n'
        '#define MKVERSION_BUILD_USER           "{10}"\n'
        "#define MKVERSION_DIRTY_STATUS         {11}\n"
        "#endif // __{1}_H__\n".format(
            info_dict["timestamp"],
            guard_str,
            info_dict["semantic_version"][0],
            info_dict["semantic_version"][1],
            info_dict["semantic_version"][2],
            info_dict["semantic_version"][3],
            info_dict["commit_str"],
            ", ".join("0x{:02x}".format(x) for x in info_dict["commit_bin_arr"]),
            len(info_dict["commit_bin_arr"]),
            info_dict["host"],
            info_dict["user"],
            info_dict["dirty_status"],
        )
    )
    Path(path).write_bytes(contents.encode("utf-8"))


def write_json(info_dict, path):
    """
    @brief Serialize version info in `info_dict` to a JSON file

    Schema for info_dict is as follows:
        {
            'semantic_version': The semantic version tuple with exactly 4 sub-components
            'dirty_status': The git repo dirty status
            'timestamp': Timestamp of the running of this script
            'commit_str': The git commit hash as an ascii hex string
            'commit_bin_arr': The git commit hash as an array of bytes
            'host': The hostname of the machine running this script
            'user': The username of who is running this script
        }
    """
    # we can more or less just serialize this directly, but we should
    # expand out the semantic version first
    json_dict = info_dict
    json_dict["semantic_version"] = {
        "major": info_dict["semantic_version"][0],
        "minor": info_dict["semantic_version"][1],
        "patch": info_dict["semantic_version"][2],
        "build": info_dict["semantic_version"][3],
    }
    Path(path).write_bytes(json.dumps(json_dict).encode("utf-8"))


def write_python(info_dict, path):
    """
    @brief Serialize version info in `info_dict` to a python file

    Schema for info_dict is as follows:
        {
            'semantic_version': The semantic version tuple with exactly 4 sub-components
            'dirty_status': The git repo dirty status
            'timestamp': Timestamp of the running of this script
            'commit_str': The git commit hash as an ascii hex string
            'commit_bin_arr': The git commit hash as an array of bytes
            'host': The hostname of the machine running this script
            'user': The username of who is running this script
        }
    """

    file_name = os.path.splitext(os.path.basename(path))[0]

    contents = (
        '"""\n'
        "auto-generated file: do not edit by hand\n"
        "generation date/time: {1}\n"
        '"""\n'
        "__version__ = {{\n"
        "    'sem_ver_major':   {2},\n"
        "    'sem_ver_minor':   {3},\n"
        "    'sem_ver_patch':   {4},\n"
        "    'sem_ver_build':   {5},\n"
        "    'git_sha_str':     '{6}',\n"
        "    'git_sha_bin_arr': [{7}],\n"
        "    'build_host':      '{8}',\n"
        "    'build_user':      '{9}',\n"
        "    'dirty_status':    {10},\n"
        "}}".format(
            file_name,
            info_dict["timestamp"],
            info_dict["semantic_version"][0],
            info_dict["semantic_version"][1],
            info_dict["semantic_version"][2],
            info_dict["semantic_version"][3],
            info_dict["commit_str"],
            ", ".join("0x{:02x}".format(x) for x in info_dict["commit_bin_arr"]),
            info_dict["host"],
            info_dict["user"],
            True if info_dict["dirty_status"].lower() == "true" else False,
        )
    )
    Path(path).write_bytes(contents.encode("utf-8"))


def main():
    """
    Main
    """
    parser = ArgumentParser()
    parser.add_argument("-c", "--cout", type=str, help="C/C++ header file output path")
    parser.add_argument("-j", "--json", type=str, help="json file output path")
    parser.add_argument("-p", "--python", type=str, help="python file output path")
    parser.add_argument(
        "-v", "--version", type=str, help="optional semantic version string"
    )
    args = parser.parse_args()

    version_tup = get_semantic_version(args.version)
    dirty_status = "true" if get_repo_dirty_status() else "false"
    timestamp = "{:%Y-%b-%d %H:%M:%S}".format(datetime.datetime.now())

    commit_hash_str = get_commit()
    commit_hash_bin_arr = [
        int(commit_hash_str[i : i + 2], 16) for i in range(0, len(commit_hash_str), 2)
    ]

    info_dict = {
        "semantic_version": version_tup,
        "dirty_status": dirty_status,
        "timestamp": timestamp,
        "commit_str": commit_hash_str,
        "commit_bin_arr": commit_hash_bin_arr,
        "host": get_host(),
        "user": get_user(),
    }

    if args.cout:
        write_c_header(info_dict, args.cout)

    if args.json:
        write_json(info_dict, args.json)

    if args.python:
        write_python(info_dict, args.python)


if __name__ == "__main__":
    main()
