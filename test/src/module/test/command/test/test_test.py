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
VERSION_H = """\
/***********************************************************************************************************************************
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
  - name: common/error
    total: 1

    coverage:
      - common/error

  - name: common/exec
    total: 1

integration:
  - name: integration
    db: true

  - name: integration/all
    total: 1
    binReq: true

performance:
  - name: performance/type
    total: 1

tool: []
"""


####################################################################################################################################
class Config:
    """What the command reads from the command line."""

    def __init__(self, repo_path, test_path, **option):
        self.repo_path = repo_path
        self.test_path = test_path
        self.vm = VM_NONE
        self.vm_arch = None
        self.vm_out = False
        self.vm_max = 1
        self.module = []
        self.test = []
        self.pg_version = "minimal"
        self.c_only = False
        self.container_only = False
        self.coverage_only = False
        self.coverage_summary = False
        self.performance = True
        self.dry_run = False
        self.build_only = False
        self.gen_only = False
        self.lint_only = False
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
def _repo_create(path, define=DEFINE):
    """Write the repository the command reads, i.e. the version, the meson build, and the test definitions."""

    result = os.path.join(path, "repo")

    file_write(os.path.join(result, "src/version.h"), VERSION_H)
    file_write(os.path.join(result, "meson.build"), MESON_BUILD)
    file_write(os.path.join(result, "test/define.yaml"), define)
    file_write(os.path.join(result, "test/uncrustify.cfg"), "")

    return result


####################################################################################################################################
def _cmd_test(config, exec_result=None, file_list=None, job_fail=None, job_start=True, job_poll=0, coverage_status=0, lint_error=0):
    """Run the command with everything it would run captured rather than run.

    Returns the status, the commands, the tests that were started, and what was written to the log."""

    command_list = []
    output = io.StringIO()

    def exec_fake(command, result_expect=0, show_output=False):
        command_list.append(command)

        # Files the repository copy is made from, since the repository written here is not a git repository to list them from
        if "ls-files" in command:
            return "" if file_list is None else "\n".join(file_list)

        result = exec_result.pop(0) if exec_result else ""

        if isinstance(result, Exception):
            raise result

        return result

    _Job.fail, _Job.start, _Job.poll, _Job.started = {} if job_fail is None else job_fail, job_start, job_poll, []

    log_init(INFO, False)

    try:
        with patch("command.test.test.exec_one", exec_fake), patch("command.test.test.TestJob", _Job):
            with patch("command.test.test.cmd_coverage", return_value=coverage_status) as coverage:
                with patch("command.test.test.cmd_lint", return_value=lint_error) as lint:
                    with patch("command.test.test.container_remove") as container_remove, redirect_stdout(output):
                        try:
                            status = cmd_test(config)
                        except ToolError as error:
                            status = str(error)

        _cmd_test.container_remove = container_remove
        _cmd_test.coverage = coverage
        _cmd_test.lint = lint
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

        # The generator is run for everything it generates, and needs nothing built first since it is python
        for generate in ("config", "error", "postgres-version", "help", "postgres"):
            assert_in("%s/build/build.py %s" % (path_repo, generate), command_list[-1])

        # The interfaces the harness uses are generated into the repository, the same as everything else, even though nothing but a
        # test build ever compiles them
        assert_true(command_list[-1].endswith("%s/build/build.py postgres-harness" % path_repo))

        assert_in("autogenerate code", output)

        # A dry run only generates what building the test list depends on, since nothing will be built from the rest
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, gen_only=True, dry_run=True))

        assert_not_in("build.py config", command_list[-1])
        assert_in("build.py help", command_list[-1])
        assert_true(command_list[-1].endswith("build.py postgres-harness"))


####################################################################################################################################
def test_test_repo_copy():
    """The repository copy holds the version controlled files, the files generated into the repository, and nothing else."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")
        path_copy = os.path.join(path_test, "repo")

        # Files git lists, the last of which is in the index but no longer in the working tree so there is nothing to copy
        file_list = ["meson.build", "src/version.h", "test/define.yaml", "test/uncrustify.cfg", "src/removed.c"]

        # A generated file is copied even though git does not list it, since a unit build compiles it from the copy
        file_write(os.path.join(path_repo, "src/command/help/help.auto.c.inc"), "help")
        file_write(os.path.join(path_repo, "src/postgres/interface.auto.c.inc"), "generated")
        file_write(os.path.join(path_repo, "test/src/harness/postgres/interface.auto.c.inc"), "harness")

        # Files that are no longer in the repository, including one in a path that nothing else is left in and one in a path that
        # has since become a file
        file_write(os.path.join(path_copy, "src/removed.c"), "removed")
        file_write(os.path.join(path_copy, "renamed/renamed.c"), "renamed")
        file_write(os.path.join(path_copy, "meson.build/renamed.c"), "renamed")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, lint_only=True), file_list=file_list)

        # The copy holds the repository as it is after the version was updated in it, since the copy is made after that
        file_copy = os.path.join(path_copy, "meson.build")
        meson_build = file_read(os.path.join(path_repo, "meson.build"))

        assert_equal(status, 0)
        assert_equal(file_read(file_copy), meson_build)
        assert_in(VERSION_DEFINE, file_read(os.path.join(path_copy, "src/version.h")))
        assert_equal(file_read(os.path.join(path_copy, "src/command/help/help.auto.c.inc")), "help")
        assert_equal(file_read(os.path.join(path_copy, "src/postgres/interface.auto.c.inc")), "generated")
        assert_equal(file_read(os.path.join(path_copy, "test/src/harness/postgres/interface.auto.c.inc")), "harness")

        # What is not in the repository is gone from the copy, along with the path it was the last file in
        assert_false(os.path.exists(os.path.join(path_copy, "src/removed.c")))
        assert_false(os.path.exists(os.path.join(path_copy, "renamed")))

        # A file with the same size and timestamp is not copied again, which is what keeps an unchanged file from being rebuilt
        stat_copy = os.stat(file_copy)
        file_write(file_copy, "x" * stat_copy.st_size)
        os.utime(file_copy, ns=(stat_copy.st_atime_ns, stat_copy.st_mtime_ns))

        _cmd_test(Config(path_repo, path_test, lint_only=True), file_list=file_list)

        assert_equal(file_read(file_copy), "x" * stat_copy.st_size)

        # A file with a different timestamp is copied again even when the size is the same
        os.utime(file_copy, ns=(stat_copy.st_atime_ns, stat_copy.st_mtime_ns + 1000000000))

        _cmd_test(Config(path_repo, path_test, lint_only=True), file_list=file_list)

        assert_equal(file_read(file_copy), meson_build)


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

        # A path that cannot be emptied says what to do about it, since a test that did not clean up after itself can leave files
        # behind that only root can remove
        file_write(os.path.join(path_test, "leftover"), "")

        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, clean_only=True),
            exec_result=[ToolError("rm: cannot remove '%s/test-0': Operation not permitted" % path_test)],
        )

        assert_in("Operation not permitted", status)
        assert_in("HINT: a test may have left files owned by root, so try 'sudo rm -rf %s/*'" % path_test, status)


####################################################################################################################################
def test_test_option_error():
    """An option that cannot be checked by the parser is checked here."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        def error(**option):
            return _cmd_test(Config(path_repo, path_test, **option))[0]

        assert_equal(error(test=[1, 2]), "only one --test can be provided")
        assert_equal(error(module=["common"], test=[1]), "--test requires a single --module naming a test module")
        assert_equal(error(vm=VM_ALL), "select a single vm to test on")
        assert_equal(error(vm="bogus"), "no definition for vm 'bogus'")

        # The test path may not be in the repository since the tests would then be part of what is being tested
        assert_in(
            "test path '%s' may not be in the repo path '%s'" % (os.path.join(path_repo, "test"), path_repo),
            _cmd_test(Config(path_repo, os.path.join(path_repo, "test")))[0],
        )

        # A selection that matches nothing is an error rather than a run that does nothing
        assert_equal(error(module=["bogus"]), "'bogus' does not match a test module")

        # A selection that matches only tests the options filter out, e.g. an integration test with no container to run it in
        assert_equal(error(module=["integration"]), "no tests were selected")

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
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm="u24", module=["integration"]))

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
    """The containers a run left behind are cleaned up before the next one."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, vm="u24", module=["common"]))

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

        # The source is linted once for the run rather than once per test, and it is the copy the tests are built from
        assert_equal(_cmd_test.lint.call_count, 1)
        assert_equal(_cmd_test.lint.call_args[0][0], os.path.join(path_test, "repo"))

        # Linting only stops the run once the source has been linted, which is how the linter is run on its own
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, lint_only=True))

        assert_equal(status, 0)
        assert_equal(_cmd_test.lint.call_count, 1)
        assert_equal(started, [])
        assert_not_in("selected", output)

        # What the linter found does not stop the run, so the tests are built and run and report on the same source before the run
        # fails on it. A syntax error the linter could only report where the source stopped making sense to it is reported by the
        # compiler at the line it is on, which the run never reaches when the linter stops it first.
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"], vm_max=2), lint_error=3)

        assert_equal(status, "3 linter error(s) (see warnings above)")
        assert_equal(started, ["common/error", "common/exec"])
        assert_in("TESTS COMPLETED SUCCESSFULLY", output)

        # There is nothing to wait for when the linter is run on its own, so it fails as soon as it has run
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, lint_only=True), lint_error=1)

        assert_equal(status, "1 linter error(s) (see warnings above)")
        assert_equal(started, [])

        # A test that fails is started again until it runs out of retries, and then the run has failed
        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common/error"], retry=1), job_fail={"common/error": 1}
        )

        assert_equal(status, 0)
        assert_equal(started, ["common/error", "common/error"])
        assert_in("1 test selected", output)
        assert_in("TESTS COMPLETED SUCCESSFULLY, 1 RETRY(IES)", output)

        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common/error"]), job_fail={"common/error": 9}
        )

        assert_equal(status, 1)
        assert_in("TESTS COMPLETED WITH 1 FAILURE(S)", output)

        # A dry run lists the tests rather than running them, even when the output of the tests was asked for
        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, vm="u24", module=["common"], dry_run=True, vm_out=True), job_start=False
        )

        assert_equal(status, 0)
        assert_equal(started, [])
        assert_in("DRY RUN COMPLETED SUCCESSFULLY", output)

        # Nothing is cleaned up and no build container is started, so a dry run leaves nothing behind for the next one to trip over
        assert_not_in("cleanup old data", output)
        assert_not_in("docker", " ".join(command_list))


####################################################################################################################################
def test_test_coverage():
    """The coverage the tests produced is merged into one report once they have all passed."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)
        path_test = os.path.join(path, "test")

        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]))

        assert_equal(status, 0)
        assert_in("tested modules have full coverage", output)

        # Coverage is merged for the tests that ran rather than for the modules that were selected
        assert_equal(_cmd_test.coverage.call_args[0][1], ["common/error", "common/exec"])

        # A module that is not fully covered fails the run, and the report says which one so it is not repeated here
        status, command_list, started, output = _cmd_test(Config(path_repo, path_test, module=["common"]), coverage_status=1)

        assert_equal(status, 1)
        assert_in("coverage report written to file://%s/test/result/coverage/coverage.html" % path_repo, output)
        assert_in("SUCCESSFULLY WITH MODULE(S) MISSING COVERAGE", output)

        # The summary is generated for the documentation, where incomplete coverage is not a failure
        status, command_list, started, output = _cmd_test(
            Config(path_repo, path_test, module=["common"], coverage_summary=True), coverage_status=1
        )

        assert_equal(status, 0)

        # There is nothing to report when coverage was not collected, when a single run was selected since that cannot cover a
        # module, or on a vm that does not collect it
        _cmd_test(Config(path_repo, path_test, module=["common"], coverage=False))

        assert_equal(_cmd_test.coverage.call_count, 0)

        _cmd_test(Config(path_repo, path_test, module=["common/error"], test=[1]))

        assert_equal(_cmd_test.coverage.call_count, 0)

        _cmd_test(Config(path_repo, path_test, vm="d12", module=["common"]))

        assert_equal(_cmd_test.coverage.call_count, 0)


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
