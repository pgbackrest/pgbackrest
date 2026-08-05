"""Test Python Test Runner.

The runner is what loads and runs a test module, so testing it means running a test module from inside one. The module it runs is
written to a temporary path and the interpreter state the runner changes -- the import path, the meta path, and the coverage
measurement -- is put back afterwards, since this interpreter carries on running the tests after the runner returns.

The coverage measurement is replaced rather than started for real, since a second measurement in the same interpreter would take
over tracing from the one already measuring this test."""

####################################################################################################################################
import io
import os
import sys
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch

from harness.test import *

from command.test.python import *

# A test module with one test that passes and one that fails, named so either can be selected
TEST_MODULE = """
def test_pass():
    pass


def test_fail():
    raise Exception("this test fails")
"""


####################################################################################################################################
def _config(path_lib, path_test, allow="", name="", coverage=None):
    """Build what the runner reads from the command line, which is what cfg_parse returns."""

    return cfg_parse(
        ["--lib=%s" % path_lib, "--test=%s" % path_test, "--allow=%s" % allow, "--name=%s" % name]
        + ([] if coverage is None else ["--coverage=%s" % coverage])
    )


####################################################################################################################################
def _run(config):
    """Run a test module, returning the status and what it wrote.

    The import path and meta path are put back since the runner adds to both and never takes them off, which is right for a process
    that exits when the module is done and wrong for this one."""

    output = io.StringIO()
    path_list = list(sys.path)
    meta_path_list = list(sys.meta_path)

    try:
        with redirect_stdout(output):
            result = test_module_run(config)
    finally:
        sys.path[:] = path_list
        sys.meta_path[:] = meta_path_list

    return result, output.getvalue()


####################################################################################################################################
def _module_write(path, content=TEST_MODULE):
    """Write a test module and return its path."""

    result = os.path.join(path, "module_test.py")

    with open(result, "w") as file:
        file.write(content)

    return result


####################################################################################################################################
def test_python_run():
    """A module runs every test in it and reports failure when any of them fail."""

    path_lib = os.path.dirname(os.path.dirname(os.path.abspath(sys.modules["harness.test"].__file__)))

    with tempfile.TemporaryDirectory() as path:
        path_test = _module_write(path)

        # A module with a failing test fails
        result, output = _run(_config(path_lib, path_test))

        assert_equal(result, 1)
        assert_in("test_pass ... ok", output)
        assert_in("test_fail ... ERROR", output)

        # A single test can be selected, which is how one test is run while debugging
        result, output = _run(_config(path_lib, path_test, name="test_pass"))

        assert_equal(result, 0)
        assert_in("Ran 1 test", output)
        assert_not_in("test_fail", output)


####################################################################################################################################
def test_python_guard():
    """A library module the test did not declare cannot be imported."""

    path_lib = os.path.dirname(os.path.dirname(os.path.abspath(sys.modules["harness.test"].__file__)))

    with tempfile.TemporaryDirectory() as path:
        # A module that was not declared is refused, with what to do about it. The import is in the test rather than at the top of
        # the module so the guard reports it as a failure rather than a load error. This runs before the case below because an
        # import that succeeded is cached in sys.modules and never reaches the guard again, which is the whole reason the runner
        # is a separate interpreter.
        path_test = _module_write(path, "def test_import():\n    import common.render\n")

        result, output = _run(_config(path_lib, path_test))

        assert_equal(result, 1)
        assert_in("'common/render' is not declared by this test module", output)
        assert_in("add it to coverage or depend in define.yaml", output)

        # A module that was declared, a package on the way to it, and something outside the library are all allowed. What the
        # declared module imports has to be declared as well, all the way down, since the guard sees every import rather than only
        # the ones the test module makes itself.
        path_test = _module_write(
            path,
            "def test_import():\n" "    import os\n" "    import common\n" "    import common.render\n",
        )

        result, output = _run(_config(path_lib, path_test, allow="common.render,common.string_id,common.error"))

        assert_equal(result, 0)


####################################################################################################################################
def test_python_coverage():
    """Coverage is measured when it is asked for and refused when it cannot be measured."""

    path_lib = os.path.dirname(os.path.dirname(os.path.abspath(sys.modules["harness.test"].__file__)))

    with tempfile.TemporaryDirectory() as path:
        path_test = _module_write(path, "def test_pass():\n    pass\n")
        path_coverage = os.path.join(path, "coverage.json")

        # The measurement is replaced so this test does not take tracing away from the measurement already running
        with patch("coverage.Coverage") as measure:
            result, output = _run(_config(path_lib, path_test, coverage=path_coverage))

            assert_equal(result, 0)

            # Measuring starts before the module is loaded and the report is written where it was asked for
            measure.return_value.start.assert_called_once_with()
            measure.return_value.stop.assert_called_once_with()
            measure.return_value.json_report.assert_called_once_with(outfile=path_coverage)

        # A coverage too old to report branch detail is refused, since a summary cannot say which line to fix
        with patch("coverage.__version__", "6.4.4"):
            result, output = _run(_config(path_lib, path_test, coverage=path_coverage))

        assert_equal(result, 2)
        assert_in("coverage 6.4.4 is too old, 6.5 or newer is required for branch detail", output)

        # Coverage that is not installed at all is refused with what to install
        with patch.dict(sys.modules, {"coverage": None}):
            result, output = _run(_config(path_lib, path_test, coverage=path_coverage))

        assert_equal(result, 2)
        assert_in("unable to measure coverage: no module named 'coverage'", output)
        assert_in("HINT: install python3-coverage", output)


####################################################################################################################################
def test_python_main():
    """The command line is what the harness passes, and main is only the wiring between it and the run."""

    path_lib = os.path.dirname(os.path.dirname(os.path.abspath(sys.modules["harness.test"].__file__)))

    with tempfile.TemporaryDirectory() as path:
        path_test = _module_write(path, "def test_pass():\n    pass\n")

        config = _config(path_lib, path_test, allow="common.render,common.log", name="test_pass")

        assert_equal(config.lib, path_lib)
        assert_equal(config.test, path_test)
        assert_equal(config.allow, "common.render,common.log")
        assert_equal(config.name, "test_pass")
        assert_is_none(config.coverage)

        # Everything main does beyond parsing is already set here, so running it again changes nothing
        output = io.StringIO()
        path_list = list(sys.path)
        meta_path_list = list(sys.meta_path)

        try:
            with patch.object(sys, "argv", ["python.py", "--lib=%s" % path_lib, "--test=%s" % path_test]):
                with redirect_stdout(output):
                    result = main()
        finally:
            sys.path[:] = path_list
            sys.meta_path[:] = meta_path_list

        assert_equal(result, 0)
        assert_true(sys.dont_write_bytecode)
        assert_in("Ran 1 test", output.getvalue())
