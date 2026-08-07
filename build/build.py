#!/usr/bin/env python3
"""pgBackRest Code Builder.

Generates the C that would be tedious and error prone to maintain by hand: the option parse tables, the error list, the help, and
the PostgreSQL interfaces. Each is generated from a declaration in src/build, which is the definition of record for what it
describes.

A generated file is only written when its content changes, so a run that generates nothing new leaves every timestamp alone and does
not make the build rebuild what depends on it.

Exit status: 0 = success, greater = error."""

####################################################################################################################################
import os
import sys

# Do not cache bytecode. The tool runs from the source tree, where a __pycache__ would show up as an unexpected binary file in the
# linter and in the distribution. This must be set before the library modules are imported below.
sys.dont_write_bytecode = True

# The library lives beside this script. Insert it first so the build modules are found before anything else on the path.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))

from command.main import cfg_load, cmd_build  # noqa: E402
from common.error import EXIT_TERM, ToolError  # noqa: E402


####################################################################################################################################
def main():
    """Generate the code named on the command line."""

    # The repository is the one this script is part of unless the caller names another, which the documentation does since it
    # generates its own option parse tables and help from its own declarations
    path_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    try:
        config = cfg_load(sys.argv[1:], path_repo)
        cmd_build(config)
    except KeyboardInterrupt:
        # A ctrl-c is what was asked for, so report it the way the C reports a signal rather than as a stack trace
        print("ERROR: terminated on signal SIGINT")

        return EXIT_TERM
    except ToolError as error:
        print("ERROR: %s" % error)

        return error.status

    return 0


if __name__ == "__main__":
    sys.exit(main())
