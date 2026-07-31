"""Command Line Interface.

The commands the harness runs, the first of which is the default so a test run never has to name one:

test        run the tests, e.g. test.py --vm=u24 --module=common
unit        build the unit test for one test module and run it, run once per test by the test run above
vm-build    build the containers the tests run in, e.g. test.py vm-build --vm=u24
code-format format the source to the project standards, or check that it is

Running the tests is by far the most common thing to do, so its options are on the main parser as well as on the test command and
the help shows them without being asked for the command.

The unit command is not in the command list the help shows since a test run is what calls it, in a container where only the
repository copy is available, rather than something to run by hand.

A command parses into the same configuration as the test run, so an option belongs after the command, which replaces whatever was
given before it."""

####################################################################################################################################
import argparse

from common.log import *


####################################################################################################################################
def _parser_common():
    """Options shared by every command."""

    result = argparse.ArgumentParser(add_help=False)

    result.add_argument(
        "--log-level", default="info", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="console log level"
    )
    result.add_argument("--no-log-timestamp", dest="log_timestamp", action="store_false", help="suppress timestamps in the log")

    return result


####################################################################################################################################
def _parser_repo(parent):
    """Options for the commands that are told where the repository is.

    A test run is not told, since it works on the repository it is part of rather than on the copy the tests are built from."""

    result = argparse.ArgumentParser(add_help=False, parents=[parent])

    result.add_argument("--repo-path", default="pgbackrest", metavar="PATH", help="code repository path")

    return result


####################################################################################################################################
def _parser_test_path(parent):
    """Options shared by the commands that work in the test path."""

    result = argparse.ArgumentParser(add_help=False, parents=[parent])

    result.add_argument("--test-path", default="test", metavar="PATH", help="path where tests are built and run")
    result.add_argument("--vm", default="none", metavar="VM", help="vm the test runs on")

    return result


####################################################################################################################################
def _parser_test(parent):
    """Options for running the tests, which select the tests, build what they need, and run them.

    These are on the main parser as well as on the test command, since running the tests is what a command line with no command
    does and it is by far the most common one, so the help must show them without being asked for the command."""

    result = argparse.ArgumentParser(add_help=False, parents=[parent])

    # Test selection
    result.add_argument("--module", action="append", default=[], metavar="MODULE", help="module or path of modules to test")
    result.add_argument("--test", action="append", type=int, default=[], metavar="TEST", help="test to run in a module")
    result.add_argument("--pg-version", default="minimal", metavar="VERSION", help="pg version integration tests run against")
    result.add_argument("--c-only", action="store_true", help="only run C tests")
    result.add_argument("--container-only", action="store_true", help="only run tests that require a container")
    result.add_argument("--coverage-only", action="store_true", help="only run tests that provide coverage")
    result.add_argument("--no-performance", dest="performance", action="store_false", help="do not run performance tests")
    result.add_argument("--dry-run", action="store_true", help="show the tests that would run without running them")

    # What to build and how
    result.add_argument("--build-only", action="store_true", help="build the binary but do not run tests")
    result.add_argument("--gen-only", action="store_true", help="only generate code")
    result.add_argument("--lint-only", action="store_true", help="only lint the source")
    result.add_argument("--no-back-trace", dest="back_trace", action="store_false", help="do not build with back trace")
    result.add_argument("--no-valgrind", dest="valgrind", action="store_false", help="do not run the C tests with valgrind")
    result.add_argument("--no-coverage", dest="coverage", action="store_false", help="do not collect coverage")
    result.add_argument("--coverage-summary", action="store_true", help="write the coverage summary for the documentation")
    result.add_argument("--profile", action="store_true", help="build with profiling and write the profile")
    result.add_argument("--scale", type=int, default=1, metavar="FACTOR", help="scale performance tests")
    result.add_argument("--tz", metavar="TZ", help="run the tests in the specified timezone")

    # Paths and logging
    result.add_argument("--clean", action="store_true", help="clean the working and result paths before running")
    result.add_argument("--clean-only", action="store_true", help="clean the working and result paths and exit")
    result.add_argument("--no-cleanup", dest="cleanup", action="store_false", help="do not clean up after a test, for debugging")
    result.add_argument(
        "--log-level-test", default="off", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="test log level"
    )
    result.add_argument("-q", "--quiet", action="store_true", help="equivalent to --log-level=off")

    # Vm
    result.add_argument("--vm-arch", metavar="ARCH", help="vm architecture (defaults to the host architecture)")
    result.add_argument("--vm-out", action="store_true", help="show the output of the tests")
    result.add_argument("--vm-max", type=int, default=1, metavar="COUNT", help="max vms to run in parallel")
    result.add_argument("--retry", type=int, default=0, metavar="COUNT", help="retry a failed test this many times")

    return result


####################################################################################################################################
def _parser_unit(command, parent):
    """The unit command, which builds one test module and runs it when it is python.

    It is left out of the command list since a test run is what calls it, so no help is given for it here. Leaving out the help is
    what keeps a command out of the list."""

    result = command.add_parser("unit", parents=[parent], description="Build a unit test.")

    result.add_argument("module", metavar="module", help="test module to build")
    result.add_argument("--vm-id", type=int, default=0, metavar="ID", help="0-based id of the vm the test runs on")
    result.add_argument("--vm-arch", metavar="ARCH", help="vm architecture (defaults to the host architecture)")
    result.add_argument("--pg-version", default="invalid", metavar="VERSION", help="pg version for integration tests")
    result.add_argument("--test", type=int, metavar="TEST", help="run only the specified test")
    result.add_argument("--test-name", metavar="LIST", help="comma separated tests to run in a python test module")
    result.add_argument("--coverage-file", metavar="PATH", help="write coverage for a python test module to this path")
    result.add_argument("--scale", type=int, default=1, metavar="FACTOR", help="scale performance tests")
    result.add_argument("--tz", metavar="TZ", help="run the test in the specified timezone")
    result.add_argument(
        "--log-level-test", default="off", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="test log level"
    )
    result.add_argument("--no-coverage", dest="coverage", action="store_false", help="do not build with coverage")
    result.add_argument("--no-back-trace", dest="back_trace", action="store_false", help="do not build with back trace")
    result.add_argument("--optimize", action="store_true", help="build with optimization")
    result.add_argument("--profile", action="store_true", help="build with profiling")


####################################################################################################################################
def _parser_vm_build(command, parent):
    """The vm-build command, which builds the containers the tests run in.

    A vm is named rather than defaulted since building every vm takes a long time, so it must be asked for."""

    result = command.add_parser(
        "vm-build", parents=[parent], help="build the vm containers", description="Build the containers the tests run in."
    )

    result.add_argument("--vm", default="none", metavar="VM", help="vm to build, or all of them")
    result.add_argument("--vm-arch", metavar="ARCH", help="vm architecture (defaults to the host architecture)")
    result.add_argument("--no-cache", dest="cache", action="store_false", help="build the containers without the cache")


####################################################################################################################################
def cli_parse(arg_list, version):
    """Build the parser and parse the command line."""

    common = _parser_common()
    repo = _parser_repo(common)
    test_path = _parser_test_path(repo)
    test = _parser_test(_parser_test_path(common))

    parser = argparse.ArgumentParser(prog="test.py", description="pgBackRest Test Harness", parents=[test])
    parser.add_argument("--version", action="version", version="pgBackRest %s Test Harness" % version)

    command = parser.add_subparsers(dest="command", metavar="command")

    # Test
    # ---------------------------------------------------------------------------------------------------------------------------
    command.add_parser("test", parents=[test], help="run the tests (default)", description="Run the tests.")

    _parser_unit(command, test_path)

    # Vm build
    # ---------------------------------------------------------------------------------------------------------------------------
    _parser_vm_build(command, common)

    # Code format
    # ---------------------------------------------------------------------------------------------------------------------------
    code_format = command.add_parser(
        "code-format",
        parents=[common],
        help="format the source to project standards",
        description="Format the source to project standards, or check that it is already formatted.",
    )
    code_format.add_argument("--check", action="store_true", help="check the formatting rather than changing it")

    result = parser.parse_args(arg_list)

    # Fill in the command a command line with no command was asking for, so nothing downstream has to know it was left out
    if result.command is None:
        result.command = "test"

    return result
