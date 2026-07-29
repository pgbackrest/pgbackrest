#!/usr/bin/env python3
"""pgBackRest Test Harness.

Builds and runs the unit tests and reports coverage. Driven by test.pl, which selects the tests to run, manages the containers, and
calls this harness for each test.

Exit status: 0 = success, 1 = the coverage command found modules missing coverage, greater = error. All output, including errors,
goes to stdout since the Perl test framework fails a test when anything is written to stderr."""

####################################################################################################################################
import os
import signal
import sys
import time
import traceback

# Send everything written to stderr to stdout instead. The Perl test framework reports only what a test writes to stdout, so an
# error raised before the handler in main() is installed, e.g. while importing below, would otherwise be lost entirely.
sys.stderr = sys.stdout

# Do not cache bytecode. The harness runs from a copy of the repository that the linter then scans, so a __pycache__ written during
# import would be reported as an unexpected binary file. This must be set before the harness modules are imported below.
sys.dont_write_bytecode = True

# The library lives beside this script. Insert it first so the harness modules are found before anything else on the path.
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "lib"))

from common.error import EXIT_ERROR, TestError  # noqa: E402

# PyYAML is the only thing these imports need outside the standard library, so a failure here is almost always a missing package.
# Coverage is also required but only by the test runner, which is a separate process and reports a missing one itself.
try:
    from command.coverage.coverage import cmd_coverage  # noqa: E402
    from command.lint.lint import cmd_lint  # noqa: E402
    from command.test.unit import cmd_unit  # noqa: E402
    from common.log import ERROR, INFO, log  # noqa: E402
    from config.config import cfg_load, project_version  # noqa: E402
except ImportError as error:
    print("unable to load the test harness: %s" % error)
    print("HINT: PyYAML is required -- install python3-yaml (Debian), python3-pyyaml (RHEL), or py3-yaml (Alpine)")
    print("HINT: rh8 needs python3.12 and python3.12-pyyaml, since the harness does not run on the platform python")

    sys.exit(EXIT_ERROR)


####################################################################################################################################
def command_run(config):
    """Run the requested command."""

    if config.command == "unit":
        cmd_unit(config)
    elif config.command == "coverage":
        return cmd_coverage(config)
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
    except TestError as error:
        print(error)

        return EXIT_ERROR

    log(INFO, "%s command begin %s: %s" % (config.command, project_version(path_harness), " ".join(sys.argv[1:])))

    try:
        result = command_run(config)
    except TestError as error:
        log(ERROR, error)
        log(INFO, "%s command end: aborted with exception" % config.command)

        return error.status
    except Exception:
        # An unexpected exception is a harness bug, so show the traceback. It goes to stdout like everything else.
        print(traceback.format_exc())

        return EXIT_ERROR

    log(INFO, "%s command end: completed successfully (%ums)" % (config.command, int((time.time() - time_begin) * 1000)))

    return result


if __name__ == "__main__":
    sys.exit(main())
