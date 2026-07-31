"""Test Command Line Interface."""

####################################################################################################################################
import io
from contextlib import redirect_stderr, redirect_stdout

from harness.test import *

from config.cli import *

# Version used for the parse, which only ever appears in the version output
VERSION = "2.60.0"


####################################################################################################################################
def _cli_parse_exit(arg_list):
    """Parse a command line that does not parse, returning the exit status and what argparse wrote.

    argparse reports and exits rather than raising, so its output is captured to keep it out of the test output."""

    output = io.StringIO()

    with assert_raises(SystemExit) as error:
        with redirect_stdout(output), redirect_stderr(output):
            cli_parse(arg_list, VERSION)

    return error.exception.code, output.getvalue()


####################################################################################################################################
def test_cli_unit_default():
    """The unit command defaults to what a plain local run needs."""

    config = cli_parse(["unit", "common/error"], VERSION)

    assert_equal(config.command, "unit")
    assert_equal(config.module, "common/error")

    # Shared by every command
    assert_equal(config.repo_path, "pgbackrest")
    assert_equal(config.log_level, "info")
    assert_true(config.log_timestamp)

    # Shared by the commands that work in the test path
    assert_equal(config.test_path, "test")
    assert_equal(config.vm, "none")

    # Specific to the unit command
    assert_equal(config.vm_id, 0)
    assert_is_none(config.vm_arch)
    assert_equal(config.pg_version, "invalid")
    assert_is_none(config.test)
    assert_is_none(config.test_name)
    assert_is_none(config.coverage_file)
    assert_equal(config.scale, 1)
    assert_is_none(config.tz)
    assert_equal(config.log_level_test, "off")
    assert_true(config.coverage)
    assert_true(config.back_trace)
    assert_false(config.optimize)
    assert_false(config.profile)


####################################################################################################################################
def test_cli_unit_option():
    """Every unit option is accepted, which is how a test run drives one test."""

    config = cli_parse(
        [
            "unit",
            "common/error",
            "--repo-path=/repo",
            "--log-level=detail",
            "--no-log-timestamp",
            "--test-path=/test",
            "--vm=u22",
            "--vm-id=2",
            "--vm-arch=aarch64",
            "--pg-version=16",
            "--test=3",
            "--test-name=test_one,test_two",
            "--coverage-file=/test/raw.json",
            "--scale=4",
            "--tz=UTC",
            "--log-level-test=debug",
            "--no-coverage",
            "--no-back-trace",
            "--optimize",
            "--profile",
        ],
        VERSION,
    )

    assert_equal(config.repo_path, "/repo")
    assert_equal(config.log_level, "detail")
    assert_false(config.log_timestamp)
    assert_equal(config.test_path, "/test")
    assert_equal(config.vm, "u22")
    assert_equal(config.vm_id, 2)
    assert_equal(config.vm_arch, "aarch64")
    assert_equal(config.pg_version, "16")
    assert_equal(config.test, 3)
    assert_equal(config.test_name, "test_one,test_two")
    assert_equal(config.coverage_file, "/test/raw.json")
    assert_equal(config.scale, 4)
    assert_equal(config.tz, "UTC")
    assert_equal(config.log_level_test, "debug")
    assert_false(config.coverage)
    assert_false(config.back_trace)
    assert_true(config.optimize)
    assert_true(config.profile)


####################################################################################################################################
def test_cli_coverage():
    """The coverage command takes every module that was run."""

    config = cli_parse(["coverage", "common/error", "common/log"], VERSION)

    assert_equal(config.command, "coverage")
    assert_equal(config.module, ["common/error", "common/log"])
    assert_false(config.coverage_summary)

    # It works in the test path so it has those options as well
    assert_equal(config.test_path, "test")

    config = cli_parse(["coverage", "common/error", "--coverage-summary"], VERSION)

    assert_true(config.coverage_summary)


####################################################################################################################################
def test_cli_test():
    """A command line with no command runs the tests, which is what a developer types."""

    config = cli_parse(["--vm=u24", "--module=common", "--module=postgres", "--test=error", "--vm-max=2"], VERSION)

    assert_is_none(config.command)
    assert_equal(config.vm, "u24")
    assert_equal(config.module, ["common", "postgres"])
    assert_equal(config.test, ["error"])
    assert_equal(config.vm_max, 2)

    # Defaults for what a plain run does
    assert_equal(config.test_path, "test")
    assert_equal(config.run, [])
    assert_equal(config.pg_version, "minimal")
    assert_equal(config.retry, 0)
    assert_equal(config.scale, 1)
    assert_is_none(config.tz)
    assert_true(config.cleanup)
    assert_true(config.coverage)
    assert_true(config.valgrind)
    assert_true(config.back_trace)
    assert_true(config.performance)
    assert_false(config.dry_run)
    assert_false(config.quiet)

    # A run works on the repository it is part of so it is never told where that is
    assert_false(hasattr(config, "repo_path"))


####################################################################################################################################
def test_cli_lint():
    """The lint command reads the repository only."""

    config = cli_parse(["lint", "--repo-path=/repo"], VERSION)

    assert_equal(config.command, "lint")
    assert_equal(config.repo_path, "/repo")


####################################################################################################################################
def test_cli_error():
    """A command line that does not parse reports and exits."""

    # An option that is not defined is rejected rather than passed on to a test
    status, output = _cli_parse_exit(["--bogus"])

    assert_equal(status, 2)
    assert_in("unrecognized arguments: --bogus", output)

    # A level outside the log levels is rejected here rather than later when it is converted
    status, output = _cli_parse_exit(["unit", "common/error", "--log-level=bogus"])

    assert_equal(status, 2)
    assert_in("invalid choice: 'bogus'", output)

    # The unit command takes exactly one module
    status, output = _cli_parse_exit(["unit"])

    assert_equal(status, 2)
    assert_in("the following arguments are required: module/test", output)

    # The version is the project version rather than one of its own
    status, output = _cli_parse_exit(["--version"])

    assert_equal(status, 0)
    assert_equal(output, "pgBackRest %s Test Harness\n" % VERSION)
