#!/usr/bin/env python3
"""pgBackRest Documentation Builder.

Builds the reference documents, the user guide, and the manual page from the same declarations the binary is generated from. The
renderers that turn them into html and markdown run separately.

Exit status: 0 = success, greater = error."""

####################################################################################################################################
import os
import sys

# Do not cache bytecode. The tool runs from the source tree, where a __pycache__ would show up as an unexpected binary file in the
# linter and in the distribution. This must be set before the library modules are imported below.
sys.dont_write_bytecode = True

# Each tool keeps its library beside itself and may use the libraries below it in the hierarchy. Insert them first, lowest last, so the
# doc modules are found before anything else on the path.
for lib in ("build", "doc"):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), lib, "lib"))

from command.build.build import cfg_load, cmd_build  # noqa: E402
from common.error import EXIT_ERROR, ToolError  # noqa: E402
from common.log import *  # noqa: E402


####################################################################################################################################
def main():
    """Build the documentation."""

    path_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # Load the configuration. Logging is not initialized until this succeeds so print any error directly.
    try:
        config = cfg_load(sys.argv[1:], path_repo)
    except ToolError as error:
        print(error)

        return EXIT_ERROR

    log_init(log_level_parse(config.log_level), config.log_timestamp)
    log(INFO, "build documentation")

    try:
        cmd_build(config.repo_path, config.var_map)
    except ToolError as error:
        log(ERROR, error)

        return error.status

    log(INFO, "build documentation end: completed successfully")

    return 0


if __name__ == "__main__":
    sys.exit(main())
