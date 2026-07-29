"""Command Line Interface.

The commands are the steps the Perl test framework drives:

unit        build the unit test for one test module, run once per test inside its container
coverage    merge the coverage produced by the unit tests and write the report
lint        run the source linter on its own"""

####################################################################################################################################
import argparse

from common.log import LEVEL_NAME


####################################################################################################################################
def _parser_common():
    """Options shared by every command."""

    result = argparse.ArgumentParser(add_help=False)

    result.add_argument("--repo-path", default="pgbackrest", metavar="PATH", help="code repository path")
    result.add_argument(
        "--log-level", default="info", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="console log level"
    )
    result.add_argument("--no-log-timestamp", dest="log_timestamp", action="store_false", help="suppress timestamps in the log")

    return result


####################################################################################################################################
def _parser_test_path(parent):
    """Options shared by the commands that work in the test path."""

    result = argparse.ArgumentParser(add_help=False, parents=[parent])

    result.add_argument("--test-path", default="test", metavar="PATH", help="path where tests are built and run")
    result.add_argument("--vm", default="none", metavar="VM", help="vm the test runs on")

    return result


####################################################################################################################################
def cli_parse(arg_list, version):
    """Build the parser and parse the command line."""

    common = _parser_common()
    test_path = _parser_test_path(common)

    parser = argparse.ArgumentParser(prog="test.py", description="pgBackRest Test Harness")
    parser.add_argument("--version", action="version", version="pgBackRest %s Test Harness" % version)
    command = parser.add_subparsers(dest="command", metavar="command", required=True)

    # Unit
    # ---------------------------------------------------------------------------------------------------------------------------
    unit = command.add_parser(
        "unit", parents=[test_path], help="build the unit test for a test module", description="Build a unit test."
    )
    unit.add_argument("module", metavar="module/test", help="test module to build")
    unit.add_argument("--vm-id", type=int, default=0, metavar="ID", help="0-based id of the vm the test runs on")
    unit.add_argument("--vm-arch", metavar="ARCH", help="vm architecture (defaults to the host architecture)")
    unit.add_argument("--pg-version", default="invalid", metavar="VERSION", help="pg version for integration tests")
    unit.add_argument("--test", type=int, metavar="RUN", help="run only the specified test run")
    unit.add_argument("--test-name", metavar="LIST", help="comma separated tests to run in a python test module")
    unit.add_argument("--coverage-file", metavar="PATH", help="write coverage for a python test module to this path")
    unit.add_argument("--scale", type=int, default=1, metavar="FACTOR", help="scale performance tests")
    unit.add_argument("--tz", metavar="TZ", help="run the test in the specified timezone")
    unit.add_argument(
        "--log-level-test", default="off", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="test log level"
    )
    unit.add_argument("--no-coverage", dest="coverage", action="store_false", help="do not build with coverage")
    unit.add_argument("--no-back-trace", dest="back_trace", action="store_false", help="do not build with back trace")
    unit.add_argument("--optimize", action="store_true", help="build with optimization")
    unit.add_argument("--profile", action="store_true", help="build with profiling")

    # Coverage
    # ---------------------------------------------------------------------------------------------------------------------------
    coverage = command.add_parser(
        "coverage",
        parents=[test_path],
        help="merge coverage and write the report",
        description="Merge the coverage produced by the unit tests and write the report.",
    )
    coverage.add_argument("module", nargs="+", metavar="module/test", help="test modules that were run")
    coverage.add_argument("--coverage-summary", action="store_true", help="write the coverage summary for the documentation")

    # Lint
    # ---------------------------------------------------------------------------------------------------------------------------
    command.add_parser("lint", parents=[common], help="lint the source", description="Run the source linter.")

    return parser.parse_args(arg_list)
