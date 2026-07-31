#!/usr/bin/env python3
"""pgBackRest Test Harness.

Builds and runs the tests and reports coverage. A run with no command selects the tests, manages the containers, and calls this
harness again with the unit command for each test, which is why a run works on the repository it is part of while every command is
told where the repository copy is.

Exit status: 0 = success, 1 = the coverage command found modules missing coverage, greater = error. All output, including errors,
goes to stdout since a test that writes anything to stderr has failed."""

####################################################################################################################################
import os
import signal
import sys
import time
import traceback

# Send everything written to stderr to stdout instead. Anything a test writes to stderr is a failure, so an error raised before
# the handler in main() is installed, e.g. while importing below, would otherwise look like one.
sys.stderr = sys.stdout

# Do not cache bytecode. The harness runs from a copy of the repository that the linter then scans, so a __pycache__ written during
# import would be reported as an unexpected binary file. This must be set before the harness modules are imported below.
sys.dont_write_bytecode = True

# Each tool keeps its library beside itself and may use the libraries below it in the hierarchy, which for the harness is all of
# them. Insert them first, lowest last, so the harness modules are found before anything else on the path.
for lib in ("build", "doc", "test"):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), lib, "lib"))

from command.code.format import cmd_code_format  # noqa: E402
from command.coverage.coverage import cmd_coverage  # noqa: E402
from command.lint.lint import cmd_lint  # noqa: E402
from command.test.test import cmd_test  # noqa: E402
from command.test.unit import cmd_unit  # noqa: E402
from common.error import EXIT_ERROR, ToolError  # noqa: E402
from common.log import *  # noqa: E402
from config.config import cfg_load  # noqa: E402
from config.project import project_version  # noqa: E402


####################################################################################################################################
def command_run(config):
    """Run the requested command."""

    # Running the tests is what a developer does, so it is what happens when no command is given
    if config.command is None:
        return cmd_test(config)

    if config.command == "unit":
        cmd_unit(config)
    elif config.command == "coverage":
        return cmd_coverage(config)
    elif config.command == "code-format":
        cmd_code_format(config)
    else:
        cmd_lint(config.repo_path)

    return 0


####################################################################################################################################
def main():
    """Main."""

    # Die silently on SIGPIPE as C programs do, rather than raising when output is piped to a command that exits early
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

    time_begin = time.time()
    path_harness = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # Load the configuration. Logging is not initialized until this succeeds so print any error directly.
    try:
        config = cfg_load(sys.argv[1:], path_harness)
    except ToolError as error:
        print(error)

        return EXIT_ERROR

    # A run with no command runs the tests, which is what it is called in the log
    command = "test" if config.command is None else config.command

    log(INFO, "%s command begin %s: %s" % (command, project_version(path_harness), " ".join(sys.argv[1:])))

    try:
        result = command_run(config)
    except ToolError as error:
        log(ERROR, error)
        log(INFO, "%s command end: aborted with exception" % command)

        return error.status
    except Exception:
        # An unexpected exception is a harness bug, so show the traceback. It goes to stdout like everything else.
        print(traceback.format_exc())

        return EXIT_ERROR

    # How long the command took is only reported when timestamps are, since the documentation runs the tests to generate output
    # that must be the same every time
    elapsed = " (%ums)" % int((time.time() - time_begin) * 1000) if config.log_timestamp else ""

    log(INFO, "%s command end: completed successfully%s" % (command, elapsed))

    return result


if __name__ == "__main__":
    sys.exit(main())
