"""Test Test Command.

The command decides what to generate, build, and run rather than doing any of it, so the commands it would run are captured the way
the container build and job tests capture theirs, and the jobs are replaced with ones that report whatever the test asked for.

The repository it works on is written here, since the command reads the version and the test definitions from it and writes the
version back."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch

from harness.test import *

from command.test.test import *
from common.error import *
from common.log import *
from common.storage import file_read, file_write
from common.vm import *

# Version as the project writes it, i.e. the components the generated defines are built from
VERSION_H = """/***********************************************************************************************************************************
Version Numbers and Names
***********************************************************************************************************************************/
#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       60
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"

#define PROJECT_VERSION                                             "old"
#define PROJECT_VERSION_NUM                                         0
"""

# The version defines as they are generated from the components above
VERSION_DEFINE = """#define PROJECT_VERSION                                             "2.60.0dev"
#define PROJECT_VERSION_NUM                                         2060000"""

# Just enough of the meson build for the version to be updated in it
MESON_BUILD = """project(
    'pgbackrest',
    ['c'],
    version: 'old',
    license: 'MIT',
)
"""

# Test definitions with a test that provides coverage, one that does not, and an integration test that needs the binary
DEFINE = """
unit:
  - name: common

    test:
      - name: error
        total: 1

        coverage:
          - common/error

      - name: exec
        total: 1

integration:
  - name: integration
    db: true

    test:
      - name: all
        total: 1
        binReq: true

performance:
  - name: performance

    test:
      - name: type
        total: 1
"""


####################################################################################################################################
class Config:
    """What the command reads from the command line."""

    def __init__(self, repo_path, test_path, **option):
        self.repo_path = repo_path
        self.test_path = test_path
        self.vm = VM_NONE
        self.vm_arch = None
        self.vm_build = False
        self.vm_force = False
        self.vm_out = False
        self.vm_max = 1
        self.module = []
        self.test = []
        self.run = []
        self.pg_version = "minimal"
        self.c_only = False
        self.container_only = False
        self.coverage_only = False
        self.coverage_summary = False
        self.performance = True
        self.dry_run = False
        self.build_only = False
        self.gen_only = False
        self.clean = False
        self.clean_only = False
        self.cleanup = True
        self.back_trace = True
        self.valgrind = True
        self.coverage = True
        self.profile = False
        self.scale = 1
        self.tz = None
        self.retry = 0
        self.log_level = INFO
        self.log_level_test = OFF
        self.log_timestamp = False

        for name, value in option.items():
            setattr(self, name, value)


####################################################################################################################################
class _Job:
    """Stand in for the job that runs a test and report what the test asked for."""

    fail = {}  # How many tries a test fails for, by test name
    start = True  # Does the job start, i.e. a dry run does not start one
    poll = 0  # How many times a job reports that it is still running before it finishes
    started = []  # Tests that were started, in the order they were started

    def __init__(self, config, run, vm_idx, vm_max, test_idx, test_max, image, show_output):
        self.name = run.module.name
        self.retry = config.retry
        self.try_idx = 0
        self.poll_idx = _Job.poll

    ################################################################################################################################
    def begin(self):
        self.try_idx += 1

        # A test that has used up its retries is not started again, and neither is one on a dry run
        if self.try_idx > self.retry + 1 or not _Job.start:
            return False

        _Job.started.append(self.name)

        return True

    ################################################################################################################################
    def end(self):
        if self.poll_idx > 0:
            self.poll_idx -= 1

            return False, False

        return True, _Job.fail.get(self.name, 0) >= self.try_idx


####################################################################################################################################
class _Exec:
    """Stand in for the coverage command, which reports whether every module was covered."""

    status = 0
    command = None

    def __init__(self, command, show_output=False):
        _Exec.command = command

    ################################################################################################################################
    def begin(self):
        pass

    ################################################################################################################################
    def end(self, wait=True):
        return _Exec.status


####################################################################################################################################
def _repo_create(path, define=DEFINE):
    """Write the repository the command reads, i.e. the version, the meson build, and the test definitions."""

    result = os.path.join(path, "repo")

    file_write(os.path.join(result, "src/version.h"), VERSION_H)
    file_write(os.path.join(result, "meson.build"), MESON_BUILD)
    file_write(os.path.join(result, "test/define.yaml"), define)
    file_write(os.path.join(result, "test/uncrustify.cfg"), "")

    return result


####################################################################################################################################
def _cmd_test(config, exec_result=None, job_fail=None, job_start=True, job_poll=0, coverage_status=0):
    """Run the command with everything it would run captured rather than run.

    Returns the status, the commands, the tests that were started, and what was written to the log."""

    command_list = []
    output = io.StringIO()

    def exec_fake(command, result_expect=0, show_output=False):
        command_list.append(command)
        result = exec_result.pop(0) if exec_result else ""

        if isinstance(result, Exception):
            raise result

        return result

    _Job.fail, _Job.start, _Job.poll, _Job.started = {} if job_fail is None else job_fail, job_start, job_poll, []
    _Exec.status, _Exec.command = coverage_status, None

    log_init(INFO, False)

    try:
        with patch("command.test.test.exec_one", exec_fake), patch("command.test.test.TestJob", _Job):
            with patch("command.test.test.Exec", _Exec), patch("command.test.test.container_build") as container_build:
                with patch("command.test.test.container_remove") as container_remove:
                    with redirect_stdout(output):
                        try:
                            status = cmd_test(config)
                        except TestError as error:
                            status = str(error)

                    _cmd_test.container_build = container_build
                    _cmd_test.container_remove = container_remove
    finally:
        log_init(INFO, True)

    return status, command_list, list(_Job.started), output.getvalue()


####################################################################################################################################
def test_test_version():
    """The version is generated from the components in the version header and written where it is needed."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        status, command_list, started, output = _cmd_test(Config(path_repo, os.path.join(path, "test"), gen_only=True))

        assert_equal(status, 0)
        assert_in(VERSION_DEFINE, file_read(os.path.join(path_repo, "src/version.h")))
        assert_in("    version: '2.60.0dev',", file_read(os.path.join(path_repo, "meson.build")))

        # A version header without the components it is generated from is an error rather than a version of nothing
        file_write(os.path.join(path_repo, "src/version.h"), '#define PROJECT_VERSION "2.60.0dev"\n')

        status, command_list, started, output = _cmd_test(Config(path_repo, os.path.join(path, "test"), gen_only=True))

        assert_equal(status, "unable to find PROJECT_VERSION_MAJOR in src/version.h")


####################################################################################################################################
def test_test_generate():
    """Code generation runs before anything else since everything else is built from what it writes."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")
        config = Config(path_repo, path_test, gen_only=True)

        status, command_list, started, output = _cmd_test(config)

        assert_equal(status, 0)

        # There is nothing built yet so the build is set up first, and the generator is run for everything it generates
        assert_in(
            "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug %s/build/none %s" % (path_test, path_repo),
            command_list[-1],
        )
        assert_in("ninja -C %s/build/none src/build-code" % path_test, command_list[-1])

        for generate in ("config", "error", "postgres-version", "postgres"):
            assert_in("/src/build-code %s %s/src" % (generate, path_repo), command_list[-1])

        assert_in("clean autogenerate code", output)

        # A dry run only generates what building the test list depends on, since nothing will be built from the rest
        file_write(os.path.join(path_test, "build/none/build.ninja"), "")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, gen_only=True, dry_run=True))

        assert_not_in("build-code config", command_list[-1])
        assert_in("build-code postgres ", command_list[-1])

        # The build is only set up when it is not there already
        assert_not_in("meson setup", command_list[-1])
        assert_not_in("clean autogenerate code", output)


####################################################################################################################################
def test_test_clean():
    """The working and result paths are emptied so a run starts from nothing."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        # Only the paths that are there are cleaned
        file_write(os.path.join(path_test, "leftover"), "")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, clean_only=True))

        assert_equal(status, 0)
        assert_equal(command_list, ["find %s -mindepth 1 -print0 | xargs -0 rm -rf" % path_test])

        # Cleaning without clean-only carries on with the run
        file_write(os.path.join(path_repo, "test/result/report"), "")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, clean=True, gen_only=True))

        assert_equal(status, 0)
        assert_in("find %s/test/result -mindepth 1 -print0 | xargs -0 rm -rf" % path_repo, command_list)
        assert_in("autogenerate code", output)


####################################################################################################################################
def test_test_option_error():
    """An option that cannot be checked by the parser is checked here."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        def error(**option):
            return _cmd_test(Config(path_repo, path_test, **option))[0]

        assert_equal(error(run=[1, 2]), "only one --run can be provided")
        assert_equal(error(test=["error"]), "only one --module can be provided when --test is specified")
        assert_equal(error(module=["common"], run=[1]), "only one --test can be provided when --run is specified")
        assert_equal(error(vm_build=True), "select a vm to build, or all of them")
        assert_equal(error(vm=VM_ALL), "select a single vm to test on")
        assert_equal(error(vm="bogus"), "no definition for vm 'bogus'")

        # The test path may not be in the repository since the tests would then be part of what is being tested
        assert_in(
            "test path '%s' may not be in the repo path '%s'" % (os.path.join(path_repo, "test"), path_repo),
            _cmd_test(Config(path_repo, os.path.join(path_repo, "test")))[0],
        )

        # A selection that matches nothing is an error rather than a run that does nothing
        assert_equal(error(module=["bogus"]), "no tests were selected")

        # Only one test can be run when nothing is cleaned up, since they would overwrite each other
        assert_equal(error(cleanup=False), "--no-cleanup is not valid when more than one test will run")


####################################################################################################################################
def test_test_build():
    """The binary is built when a test needs it and the build path is set up either way."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        # A unit test needs no binary but its build path is mounted into the container, so the path is still set up
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"], build_only=False))

        assert_equal(status, 0)
        assert_in(
            "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug %s/build/none %s" % (path_test, path_repo),
            command_list,
        )
        assert_not_in("ninja -C %s/build/none src/pgbackrest 2>&1" % path_test, " ".join(command_list))
        assert_equal(started, ["common/error", "common/exec"])

        # An integration test runs the binary so it is built. The build is already set up so only the binary is built.
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm=VM_U24, module=["integration"]))

        assert_in("ninja -C %s/build/u24 src/pgbackrest 2>&1" % path_test, " ".join(command_list))

        # The build runs in the build container, which is started and shut down around it
        assert_in("docker run -itd -h test-build --name=test-build", " ".join(command_list))
        assert_in("docker rm -f test-build", command_list)

        # Building only builds the binary and runs nothing
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, build_only=True))

        assert_equal(status, 0)
        assert_equal(started, [])

        # A build that is already set up and needs no binary has nothing to do
        file_write(os.path.join(path_test, "build/none/build.ninja"), "")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]))

        assert_not_in("meson setup", " ".join(command_list))
        assert_not_in("src/pgbackrest", " ".join(command_list))


####################################################################################################################################
def test_test_container():
    """The containers are built when they were asked for and cleaned up before a run."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm=VM_U24, vm_build=True))

        assert_equal(status, 0)
        assert_equal(_cmd_test.container_build.call_count, 1)

        # Nothing else is done since the containers are all that was asked for
        assert_equal(command_list, [])

        # Every vm can be built at once, which is the only thing that can be done for all of them
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm=VM_ALL, vm_build=True))

        assert_equal(status, 0)
        assert_equal(_cmd_test.container_build.call_count, 1)

        # A run on a vm removes whatever the last run left behind
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm=VM_U24, module=["common"]))

        assert_equal(_cmd_test.container_remove.call_args[0][0], "test-([0-9]+|build)")
        assert_in("cleanup old data and containers", output)


####################################################################################################################################
def test_test_run():
    """Every vm is kept busy until there is nothing left to run."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        # More tests than vms, so a vm picks up the next test as soon as it is free
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"], vm_max=2), job_poll=1)

        assert_equal(status, 0)
        assert_equal(started, ["common/error", "common/exec"])
        assert_in("2 tests selected", output)
        assert_in("TESTS COMPLETED SUCCESSFULLY", output)

        # A test that fails is started again until it runs out of retries, and then the run has failed
        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common"], test=["error"], retry=1), job_fail={"common/error": 1}
        )

        assert_equal(status, 0)
        assert_equal(started, ["common/error", "common/error"])
        assert_in("1 test selected", output)
        assert_in("TESTS COMPLETED SUCCESSFULLY, 1 RETRY(IES)", output)

        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common"], test=["error"]), job_fail={"common/error": 9}
        )

        assert_equal(status, 1)
        assert_in("TESTS COMPLETED WITH 1 FAILURE(S)", output)

        # A dry run lists the tests rather than running them
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, dry_run=True), job_start=False)

        assert_equal(status, 0)
        assert_equal(started, [])
        assert_in("DRY RUN COMPLETED SUCCESSFULLY", output)


####################################################################################################################################
def test_test_coverage():
    """The coverage the tests produced is merged into one report once they have all passed."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]))

        assert_equal(status, 0)
        assert_in("test.py coverage --log-level=warn --vm=none", _Exec.command)
        assert_in("common/error common/exec", _Exec.command)
        assert_in("tested modules have full coverage", output)

        # A module that is not fully covered fails the run, and the report says which one so it is not repeated here
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]), coverage_status=1)

        assert_equal(status, 1)
        assert_in("SUCCESSFULLY WITH MODULE(S) MISSING COVERAGE", output)

        # The summary is generated for the documentation, where incomplete coverage is not a failure
        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common"], coverage_summary=True), coverage_status=1
        )

        assert_equal(status, 0)
        assert_in(" --coverage-summary ", _Exec.command)

        # Anything worse than incomplete coverage is an error
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]), coverage_status=2)

        assert_equal(status, "coverage command failed")

        # There is nothing to report when coverage was not collected, when a single run was selected since that cannot cover a
        # module, or on a vm that does not collect it
        _cmd_test(Config(path_repo, path_test, module=["common"], coverage=False))

        assert_is_none(_Exec.command)

        _cmd_test(Config(path_repo, path_test, module=["common"], test=["error"], run=[1]))

        assert_is_none(_Exec.command)

        _cmd_test(Config(path_repo, path_test, vm=VM_D12, module=["common"]))

        assert_is_none(_Exec.command)


####################################################################################################################################
def test_test_profile():
    """Profiling turns off everything that would skew the timing."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        config = Config(path_repo, os.path.join(path, "test"), module=["common"], profile=True)

        _cmd_test(config)

        assert_false(config.back_trace)
        assert_false(config.valgrind)
        assert_false(config.coverage)

        # The coverage summary is built from the C tests that provide coverage, so it selects them
        config = Config(path_repo, os.path.join(path, "test"), module=["common"], coverage_summary=True)

        _cmd_test(config)

        assert_true(config.coverage_only)
        assert_true(config.c_only)
