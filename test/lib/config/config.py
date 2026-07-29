"""Configuration Load.

Parses the command line, resolves paths, and initializes logging."""

import os
import re

from common.error import TestError
from common.log import log_init, log_level_parse
from common.storage import file_read
from config.cli import cli_parse


####################################################################################################################################
def project_version(path_repo):
    """Read the project version from src/version.h in the repository this script was checked out into.

    The harness always runs from a copy of the repository so the version is never ambiguous."""

    match = re.search(r'^#define PROJECT_VERSION\s+"([^"]+)"', file_read(os.path.join(path_repo, "src/version.h")), re.M)

    if match is None:
        raise TestError("unable to find PROJECT_VERSION in src/version.h")

    return match.group(1)


####################################################################################################################################
def cfg_load(arg_list, path_harness):
    """Parse the command line and apply the rules that cannot be expressed in the parser."""

    config = cli_parse(arg_list, project_version(path_harness))

    # Make relative paths absolute so the harness does not depend on the working directory
    config.repo_path = os.path.abspath(config.repo_path)

    # The repository is only ever read from so it must already exist. Checking here keeps a mistyped path from looking like success
    # rather than an error, e.g. the linter would find no files to check and report that everything passed.
    if not os.path.isdir(config.repo_path):
        raise TestError("repo path '%s' does not exist" % config.repo_path)

    if hasattr(config, "test_path"):
        config.test_path = os.path.abspath(config.test_path)

    # Convert log levels to ids now that they are known to be valid
    config.log_level = log_level_parse(config.log_level)

    if hasattr(config, "log_level_test"):
        config.log_level_test = log_level_parse(config.log_level_test)

    log_init(config.log_level, config.log_timestamp)

    return config
