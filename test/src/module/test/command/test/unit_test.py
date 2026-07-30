"""Test Unit Command.

The build and the commands it runs are replaced, since what this module does is decide what to build and what to run rather than
build or run it. Everything is driven through the command itself rather than reaching for the helpers it is made of, which is how
the C tests are written."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch

from harness.test import *

from command.test.unit import *
from common.error import *
from common.log import *

# A define file with the C and python tests the command has to tell apart
DEFINE = """
unit:
  - name: common

    test:
      - name: error
        total: 1

        coverage:
          - common/error

      - name: release
        total: 1
        define: -DNDEBUG

        coverage:
          - common/release

      - name: bogus
        total: 1
        define: -DBOGUS

        coverage:
          - common/bogus

  - name: test
    lang: python

    test:
      - name: common/log

        coverage:
          - test/common/log
          - test/common/logInternal: noCode

        depend:
          - test/common/error

        include:
          - test/common/render

integration: []
performance:
  - name: performance

    test:
      - name: type
        total: 1

        coverage:
          - performance/type
"""


####################################################################################################################################
class Config:
    """What the unit command reads from the command line."""

    def __init__(self, repo_path, test_path, module, **option):
        self.repo_path = repo_path
        self.test_path = test_path
        self.module = module
        self.vm = "none"
        self.vm_id = 0
        self.vm_arch = None
        self.pg_version = "invalid"
        self.test = None
        self.test_name = None
        self.coverage_file = None
        self.scale = 1
        self.tz = None
        self.log_level_test = OFF
        self.log_timestamp = True
        self.coverage = True
        self.back_trace = True
        self.optimize = False
        self.profile = False

        for name, value in option.items():
            setattr(self, name, value)


####################################################################################################################################
def _repo_create(path, define=DEFINE, test_module=("test/common/log",)):
    """Write the repository the command reads, i.e. the define file and the python test modules it names."""

    result = os.path.join(path, "repo")

    os.makedirs(os.path.join(result, "test/src/module/test/common"), exist_ok=True)

    with open(os.path.join(result, "test/define.yaml"), "w") as file:
        file.write(define)

    for name in test_module:
        with open(os.path.join(result, "test/src/module", name + "_test.py"), "w") as file:
            file.write("def test_pass():\n    pass\n")

    return result


####################################################################################################################################
def _cmd_unit(config, exec_result=None):
    """Run the unit command with the build and the commands it runs replaced.

    Returns the commands that would have been run, what was written, and the error raised, if any. The exec results are returned in
    order, and an exception in the list is raised instead."""

    command_list = []
    output = io.StringIO()
    error = None

    def exec_fake(command, result_expect=0):
        command_list.append(command)
        result = exec_result.pop(0) if exec_result else ""

        if isinstance(result, Exception):
            raise result

        return result

    log_init(WARN, False)

    try:
        with patch("command.test.unit.exec_one", exec_fake), patch("command.test.unit.TestBuild") as build:
            build.return_value.path_unit = os.path.join(config.test_path, "unit-0/none")
            build.return_value.path_unit_build = os.path.join(config.test_path, "unit-0/none/build")

            try:
                with redirect_stdout(output):
                    cmd_unit(config)
            except TestError as exception:
                error = str(exception)

            _cmd_unit.build = build
    finally:
        log_init(INFO, True)

    return command_list, output.getvalue(), error


####################################################################################################################################
def test_unit_python():
    """A python module has nothing to build so it is run where it is, with only what it declared importable."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        command_list, output, error = _cmd_unit(Config(path_repo, path, "test/common/log"), ["ran the tests\n"])

        assert_is_none(error)

        # The output comes back as it was written rather than through the log, which would indent it as a continuation
        assert_equal(output, "ran the tests\n")

        # The runner is given the code modules the test covers, what it declared, and what earlier tests covered. A module with no
        # code is not importable so it is not in the list.
        assert_equal(len(command_list), 1)
        assert_in("--allow='common.log,common.error,common.render'", command_list[0])

        # There is no coverage file so coverage is not measured, and no test name so every test runs
        assert_in("--name=''", command_list[0])
        assert_not_in("--coverage=", command_list[0])


####################################################################################################################################
def test_unit_python_option():
    """A single test can be selected and coverage is written where the caller asks for it."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_coverage = os.path.join(path, "result/coverage/raw/test-log.json")

        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "test/common/log", test_name="test_pass", coverage_file=path_coverage)
        )

        assert_is_none(error)
        assert_in("--name='test_pass'", command_list[0])
        assert_in("--coverage='%s'" % path_coverage, command_list[0])

        # The path is created since the harness runs from a copy of the repository and the report is built from the original
        assert_true(os.path.isdir(os.path.dirname(path_coverage)))

        # Coverage is not measured when the build is not measuring it, even when a file was named
        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "test/common/log", coverage=False, coverage_file=path_coverage)
        )

        assert_not_in("--coverage=", command_list[0])


####################################################################################################################################
def test_unit_python_error():
    """A python test module that is not there is an error that names the file that was expected."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path, test_module=())

        command_list, output, error = _cmd_unit(Config(path_repo, path, "test/common/log"))

        assert_equal(error, "unable to find test module '%s/test/src/module/test/common/log_test.py'" % path_repo)
        assert_equal(command_list, [])


####################################################################################################################################
def test_unit_build():
    """A C module is set up and built, with the architecture taken from the machine when it was not given."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        command_list, output, error = _cmd_unit(Config(path_repo, path, "common/error"))

        assert_is_none(error)

        # Meson is set up since there is nothing built yet, then ninja builds it
        assert_in("meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug -Db_coverage=true", command_list[0])
        assert_in("ninja -C '%s/unit-0/none/build'" % path, command_list[1])

        # The architecture comes from the machine when it was not given
        assert_equal(_cmd_unit.build.call_args[0][2], host_arch())

        # An architecture that was given is used as it is
        _cmd_unit(Config(path_repo, path, "common/error", vm_arch="ppc64le"))

        assert_equal(_cmd_unit.build.call_args[0][2], "ppc64le")


####################################################################################################################################
def test_unit_build_type():
    """The build type is release when the module cannot be built with debug, else debug."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        # A module that sets a define
        command_list, output, error = _cmd_unit(Config(path_repo, path, "common/release", vm_arch="x86_64"))

        assert_is_none(error)
        assert_in("-Dbuildtype=release", command_list[0])

        # A performance test, which is timed so it cannot be built with debug either
        command_list, output, error = _cmd_unit(Config(path_repo, path, "performance/type", vm_arch="x86_64"))

        assert_in("-Dbuildtype=release", command_list[0])

        # Profiling, which is also release, and turns coverage off since the two cannot both be measured
        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "common/error", vm_arch="x86_64", profile=True, coverage=False)
        )

        assert_in("-Dbuildtype=release -Db_coverage=false", command_list[0])

        # A define other than the one that turns debug off is not something the build knows what to do with
        command_list, output, error = _cmd_unit(Config(path_repo, path, "common/bogus", vm_arch="x86_64"))

        assert_equal(error, "unexpected define '-DBOGUS'")


####################################################################################################################################
def test_unit_build_reuse():
    """A unit path that was already set up is reconfigured rather than set up again, and stale data is cleared out."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_build = os.path.join(path, "unit-0/none/build")
        path_object = os.path.join(path_build, "test-unit.p")

        os.makedirs(path_object)

        for name in ("build.ninja", "gmon.out"):
            with open(os.path.join(path_build, name), "w") as file:
                file.write("")

        # Coverage and profile data from the last run, which would otherwise be counted again
        for name in ("stale.gcda", "stale.gcno"):
            with open(os.path.join(path_object, name), "w") as file:
                file.write("")

        command_list, output, error = _cmd_unit(Config(path_repo, path, "common/error", vm_arch="x86_64"))

        assert_is_none(error)
        assert_in("meson configure -Dbuildtype=debug -Db_coverage=true", command_list[0])

        # The coverage data is gone but what the build needs to link is left alone
        assert_false(os.path.exists(os.path.join(path_object, "stale.gcda")))
        assert_true(os.path.exists(os.path.join(path_object, "stale.gcno")))
        assert_false(os.path.exists(os.path.join(path_build, "gmon.out")))


####################################################################################################################################
def test_unit_build_retry():
    """A build that fails is retried once from a clean path, since a stale build path is the usual cause."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_unit = os.path.join(path, "unit-0/none")
        path_stale = os.path.join(path_unit, "stale")

        os.makedirs(path_stale)

        # The first ninja fails and the retry succeeds
        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "common/error", vm_arch="x86_64"), ["", TestError("ninja said no"), "", ""]
        )

        assert_is_none(error)
        assert_in("build failed for unit common/error -- will retry: ninja said no", output)

        # The unit path was emptied before the retry
        assert_false(os.path.exists(path_stale))
        assert_true(os.path.isdir(path_unit))

        # A second failure is the end of it, with the error from the build rather than from the retry
        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "common/error", vm_arch="x86_64"),
            ["", TestError("ninja said no"), "", TestError("ninja said no again")],
        )

        assert_equal(error, "build failed for unit common/error: ninja said no again")


####################################################################################################################################
def test_unit_build_retry_clean():
    """A unit path that was never created is nothing to empty, which is the first build on a clean test path."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_unit = os.path.join(path, "unit-0/none")

        assert_false(os.path.exists(path_unit))

        command_list, output, error = _cmd_unit(
            Config(path_repo, path, "common/error", vm_arch="x86_64"), ["", TestError("ninja said no"), "", ""]
        )

        assert_is_none(error)
        assert_true(os.path.isdir(path_unit))


####################################################################################################################################
def test_unit_build_retry_mode():
    """A unit path a test left unreadable is reset before it is emptied, since a test may run as another user."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        # The path cannot be removed until the mode is reset, which is the second thing tried
        with patch("command.test.unit.shutil.rmtree", side_effect=[OSError("permission denied"), None]):
            command_list, output, error = _cmd_unit(
                Config(path_repo, path, "common/error", vm_arch="x86_64"), ["", TestError("ninja said no"), "", ""]
            )

        assert_is_none(error)
        assert_in("chmod -R 777 '%s/unit-0/none'" % path, command_list)
