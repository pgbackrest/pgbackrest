"""Configuration Load.

Parses the command line, resolves paths, and initializes logging."""

import os

from common.error import ToolError
from common.log import *
from config.cli import cli_parse
from config.project import project_version


####################################################################################################################################
def cfg_load(arg_list, path_harness):
    """Parse the command line and apply the rules that cannot be expressed in the parser."""

    config = cli_parse(arg_list, project_version(path_harness))

    # A test run works on the repository it is part of. A command is told where the repository is, since it runs from the copy the
    # tests are built from. Relative paths are made absolute so nothing depends on the working directory.
    config.repo_path = os.path.abspath(config.repo_path) if hasattr(config, "repo_path") else path_harness
    config.test_path = os.path.abspath(config.test_path)

    # The repository is only ever read from so it must already exist. Checking here keeps a mistyped path from looking like success
    # rather than an error, e.g. the linter would find no files to check and report that everything passed.
    if not os.path.isdir(config.repo_path):
        raise ToolError("repo path '%s' does not exist" % config.repo_path)

    # Convert log levels to ids now that they are known to be valid
    config.log_level = OFF if config.quiet else log_level_parse(config.log_level)
    config.log_level_test = log_level_parse(config.log_level_test)

    log_init(config.log_level, config.log_timestamp)

    return config
