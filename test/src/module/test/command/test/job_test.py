"""Test Test Job.

The job decides what to run and reports what happened rather than running anything itself, so what it would have run is captured
instead, the same way the container build test captures docker. The test path and the user vary by machine so both are replaced
with tokens before comparing, as the build test does."""

####################################################################################################################################
import io
import os
import re
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch

from harness.test import *

from command.test.define import TEST_LANG_C, TEST_LANG_PYTHON, TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE
from command.test.define import TEST_TYPE_TOOL, TEST_TYPE_UNIT
from command.test.define import TestDefModule
from command.test.job import *
from command.test.list import TestRun
from common.error import ToolError
from common.log import *
from common.storage import file_write
from common.user import user_name
from common.vm import *

# Image a test that runs in a container is started from
IMAGE = "ghcr.io/pgbackrest/test:u24-test-x86_64"

# What a C unit test runs, i.e. the harness that builds it and then the binary itself under valgrind. The binary is run rather than
# left to the harness so the run can be wrapped in valgrind and its output copied to both streams.
COMMAND_C = """python3 [PATH]/repo/test/test.py unit --repo-path=[PATH]/repo --test-path=[PATH] --log-level=info \
--log-level-test=off --vm=none --vm-id=0 --no-log-timestamp --scale=1 common/error && \\
exec 3>&1 && \\
{ valgrind -q --gen-suppressions=all --exit-on-first-error=yes --leak-check=full --leak-resolution=high --error-exitcode=25 \
[PATH]/unit-0/none/build/test-unit 2>&1 1>&3; echo $? > [PATH]/unit-0/none/result; } | tee /dev/stderr && \\
exit $(cat [PATH]/unit-0/none/result)"""

# What a python test runs. There is no binary so the harness runs the test itself, and it writes its own coverage since there is no
# gcov step for it afterward.
COMMAND_PYTHON = """python3 [PATH]/repo/test/test.py unit --repo-path=[PATH]/repo --test-path=[PATH] --log-level=info \
--log-level-test=off --vm=none --vm-id=0 --no-log-timestamp --scale=1 \
--coverage-file=[PATH]/repo/test/result/coverage/raw/test-common-log.json test/common/log"""


####################################################################################################################################
class Config:
    """What the job reads from the command line."""

    def __init__(self, test_path, **option):
        self.repo_path = os.path.join(test_path, "repo")
        self.test_path = test_path
        self.vm_arch = None
        self.log_level = INFO
        self.log_level_test = OFF
        self.log_timestamp = False
        self.dry_run = False
        self.vm_out = False
        self.cleanup = True
        self.retry = 0
        self.scale = 1
        self.tz = None
        self.profile = False
        self.valgrind = True
        self.coverage = True
        self.back_trace = True

        for name, value in option.items():
            setattr(self, name, value)


####################################################################################################################################
class _Exec:
    """Stand in for the process that runs the test and report what the test asked for.

    The result is set on the class since the job creates the process itself, so there is nothing to pass it through."""

    status = 0
    output = ""
    error = ""
    poll = 0  # How many times the process reports that it is still running before it finishes

    def __init__(self, command, show_output=False):
        self.command = command
        self.show_output = show_output

    ################################################################################################################################
    def begin(self):
        pass

    ################################################################################################################################
    def end(self, wait=True):
        # Report that the process is still running as many times as the test asked for
        if self.poll > 0:
            self.poll -= 1

            return None

        return self.status


####################################################################################################################################
def _run(name="common/error", vm=VM_NONE, type=TEST_TYPE_UNIT, lang=TEST_LANG_C, pg_version=None, test_list=None, total=0):
    """Build a test to run."""

    module = TestDefModule(name, type)
    module.lang = lang
    module.total = total

    return TestRun(module, vm, pg_version, test_list)


####################################################################################################################################
def _job(config, run=None, vm_idx=0, vm_max=1, test_idx=0, test_max=1, show_output=False):
    """Build a job to run a test."""

    return TestJob(config, _run() if run is None else run, vm_idx, vm_max, test_idx, test_max, IMAGE, show_output)


####################################################################################################################################
def _capture(job, action, status=0, output="", error="", poll=0, fail=None):
    """Run something on a job with the commands it would run captured rather than run.

    Returns what the action returned, the commands, and what was written to the log. Naming a command in fail makes it report that it
    could not be run."""

    command_list = []
    log_output = io.StringIO()

    def exec_fake(command, result_expect=0, show_output=False):
        command_list.append(command)

        if fail is not None and command.startswith(fail):
            raise ToolError("%s terminated unexpectedly [1]" % fail)

        return ""

    _Exec.status, _Exec.output, _Exec.error, _Exec.poll = status, output, error, poll

    # Timestamps are suppressed so the log can be compared, and the level is raised so the detail a test writes is included
    log_init(DETAIL, False)

    try:
        with patch("command.test.job.exec_one", exec_fake), patch("command.test.job.Exec", _Exec):
            with patch("command.test.job.container_remove", lambda expression: command_list.append("remove " + expression)):
                with redirect_stdout(log_output):
                    result = action(job)
    finally:
        log_init(INFO, True)

    return result, command_list, log_output.getvalue()


####################################################################################################################################
def _expect(text, config):
    """Fill in the test path and the user the tests run as."""

    return text.replace("[PATH]", config.test_path).replace("[USER]", user_name())


####################################################################################################################################
def test_job_command():
    """A C test builds itself through the harness and then runs the binary it built."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path)
        job = _job(config)

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_true(started)
        assert_equal(job.exec.command, _expect(COMMAND_C, config))

        # Nothing is run to start the test since there is no container for it
        assert_equal(command_list, [])

        # The test is listed at detail, since at info the log would be a list of tests that have not finished yet
        assert_equal(output, "P00 DETAIL: P1-T1/1 - vm=none, module=common/error\n")

        # The paths the test writes to are created, and the data path for everything but a performance test
        assert_true(os.path.isdir(os.path.join(path, "test-0")))
        assert_true(os.path.isdir(os.path.join(path, "unit-0/none")))
        assert_true(os.path.isdir(os.path.join(path, "data-0")))

        # A python test has nothing to build so the harness runs it, and it writes its own coverage
        job = _job(config, _run(name="test/common/log", type=TEST_TYPE_TOOL, lang=TEST_LANG_PYTHON))

        _capture(job, lambda job: job.begin())

        assert_equal(job.exec.command, _expect(COMMAND_PYTHON, config))


####################################################################################################################################
def test_job_command_option():
    """The options a test was given are passed on to the harness that runs it."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path, vm_arch="ppc64le", tz="America/New_York", scale=4, back_trace=False, log_timestamp=True)
        job = _job(config, _run(pg_version="15", test_list=[2, 1]))

        _capture(job, lambda job: job.begin())

        assert_in(" --vm-arch=ppc64le --vm-id=0 --test=2 --test=1 --tz='America/New_York' --scale=4", job.exec.command)
        assert_in(" --pg-version=15 --no-back-trace ", job.exec.command)

        # Timestamps are only suppressed when they were suppressed here, since the point is a reproducible log
        assert_not_in("--no-log-timestamp", job.exec.command)

        # A run is listed with the sub-tests and the PostgreSQL version it was selected for
        assert_equal(job.description, "P1-T1/1 - vm=none, module=common/error, test=1,2, pg-version=15")

        # Coverage is not collected for a profile run since the instrumentation would skew the timing
        job = _job(Config(path, profile=True), _run())

        _capture(job, lambda job: job.begin())

        assert_in(" --profile ", job.exec.command)
        assert_in(" --no-coverage ", job.exec.command)

        # Nor for a performance test, for the same reason
        job = _job(Config(path), _run(name="performance/type", type=TEST_TYPE_PERFORMANCE))

        _capture(job, lambda job: job.begin())

        assert_in(" --no-coverage ", job.exec.command)

        # Nor on a vm that cannot collect it
        job = _job(Config(path), _run(vm="d12"))

        _capture(job, lambda job: job.begin())

        assert_in(" --no-coverage ", job.exec.command)


####################################################################################################################################
def test_job_valgrind():
    """A C test runs under valgrind, with the suppressions for its vm when there are any."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path)

        # A performance test is timed so valgrind would make the result meaningless
        job = _job(config, _run(name="performance/type", type=TEST_TYPE_PERFORMANCE))

        _capture(job, lambda job: job.begin())

        assert_not_in("valgrind", job.exec.command)

        # Valgrind can be turned off to save time
        job = _job(Config(path, valgrind=False))

        _capture(job, lambda job: job.begin())

        assert_not_in("valgrind", job.exec.command)

        # The suppressions for the vm are used when the repository has them
        path_suppress = os.path.join(path, "repo/test/src/valgrind.suppress.none")
        file_write(path_suppress, "")

        job = _job(config)

        _capture(job, lambda job: job.begin())

        assert_in("--gen-suppressions=all --suppressions=%s --exit-on-first-error" % path_suppress, job.exec.command)


####################################################################################################################################
def test_job_container():
    """A test on a vm runs in a container of its own, which is started here and removed when the test is done."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path, vm_arch="x86_64")
        job = _job(config, _run(vm="u24"), vm_idx=1, vm_max=2, test_idx=3, test_max=12)

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_true(started)
        assert_equal(
            command_list,
            [
                _expect(
                    "docker run --platform linux/x86_64 -itd -h u24-test --name=test-1 -v [PATH]/test-1:[PATH]/test-1"
                    " -v [PATH]/unit-1/u24:[PATH]/unit-1/u24 -v [PATH]/data-1:[PATH]/data-1"
                    " -v [PATH]/build/u24:[PATH]/build/u24:ro -v [PATH]/ccache:[PATH]/ccache -e CCACHE_DIR=[PATH]/ccache"
                    " -v [PATH]/repo:[PATH]/repo -v [PATH]/repo:[PATH]/repo " + IMAGE,
                    config,
                )
            ],
        )

        # The test itself runs in the container it was started in
        assert_true(job.exec.command.startswith(_expect("docker exec -i -u [USER] test-1 bash -l -c '\\\n", config)))

        # The vm and the test are numbered from one and padded out so the list lines up
        assert_equal(job.description, "P2-T04/12 - vm=u24, module=common/error")

        # An integration test starts its own containers and has nothing built for it, so neither is done here
        job = _job(config, _run(name="integration/all", type=TEST_TYPE_INTEGRATION, vm="u24", test_list=[1]))

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_equal(command_list, [])
        assert_false(os.path.exists(os.path.join(path, "unit-0/u24")))

        # It runs the binary that was built for none rather than one built for the vm, and outside a container
        assert_true(job.exec.command.startswith("python3 "))
        assert_in(" --vm=u24 ", job.exec.command)
        assert_in(_expect("[PATH]/unit-0/none/build/test-unit", config), job.exec.command)


####################################################################################################################################
def test_job_dry_run():
    """A dry run lists the test rather than running it, unless the output was asked for."""

    with tempfile.TemporaryDirectory() as path:
        job = _job(Config(path, dry_run=True))

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_false(started)
        assert_equal(output, "P00   INFO: P1-T1/1 - vm=none, module=common/error\n")

        # Nothing is created since nothing will run
        assert_false(os.path.exists(os.path.join(path, "test-0")))

        # Asking for the output of a dry run runs the test after all, which is a way to see more about what it would do
        job = _job(Config(path, dry_run=True, vm_out=True))

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_true(started)
        assert_equal(output, "P00 DETAIL: P1-T1/1 - vm=none, module=common/error\n")


####################################################################################################################################
def test_job_end():
    """A test that passed is reported with how long it took."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path)
        job = _job(config)

        _capture(job, lambda job: job.begin())

        # The test is not done until the process is, which is how the driver knows it can start another one
        (done, fail), command_list, output = _capture(job, lambda job: job.end(), poll=1)

        assert_false(done)
        assert_false(fail)
        assert_equal(command_list, [])

        (done, fail), command_list, output = _capture(job, lambda job: job.end())

        assert_true(done)
        assert_false(fail)
        assert_equal(output, "P00   INFO: P1-T1/1 - vm=none, module=common/error\n")

        # The coverage the test left behind is written where the report is built from, and then the test path is removed
        assert_equal(
            command_list,
            [
                _expect(
                    "gcov --json-format --stdout --branch-probabilities [PATH]/unit-0/none/build/test-unit@exe/test.c.gcda"
                    " > [PATH]/repo/test/result/coverage/raw/common-error.json",
                    config,
                ),
                _expect("chmod -R 700 [PATH]/test-0/* 2>&1;rm -rf [PATH]/test-0", config),
            ],
        )

        # Meson has named the directory it compiled the test into both ways, so the other name is used when it is there
        file_write(os.path.join(path, "unit-0/none/build/test-unit.p/test.c.gcda"), "")

        job = _job(config)

        _capture(job, lambda job: job.begin())
        (done, fail), command_list, output = _capture(job, lambda job: job.end())

        assert_in("[PATH]/unit-0/none/build/test-unit.p/test.c.gcda".replace("[PATH]", path), command_list[0])


####################################################################################################################################
def test_job_end_cleanup():
    """The test path is removed even when writing the coverage fails, since this is the last chance to remove it.

    A test leaves files behind that only the user it ran as can remove, so a path left here is a path every later run fails to clean.
    """

    with tempfile.TemporaryDirectory() as path:
        config = Config(path)
        job = _job(config)

        _capture(job, lambda job: job.begin())

        def end(job):
            try:
                job.end()
            except ToolError as exception:
                return str(exception)

            return None

        error, command_list, output = _capture(job, end, fail="gcov")

        assert_equal(error, "gcov terminated unexpectedly [1]")
        assert_equal(command_list[-1], _expect("chmod -R 700 [PATH]/test-0/* 2>&1;rm -rf [PATH]/test-0", config))


####################################################################################################################################
def test_job_end_output():
    """What the test wrote is reported when it was asked for and when the test failed."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path, vm_out=True, coverage=False, cleanup=False, log_timestamp=True)
        job = _job(config)

        _capture(job, lambda job: job.begin())
        (done, fail), command_list, output = _capture(job, lambda job: job.end(), output="all good\n")

        assert_true(done)
        assert_false(fail)

        # Nothing is cleaned up so the result can be looked at, and the elapsed time is reported
        assert_equal(command_list, [])
        assert_true(re.match(r"^P00   INFO: P1-T1/1 - vm=none, module=common/error \(\d+\.\d\ds\):", output))
        assert_in("all good", output)

        # Output that arrived as it was written is not written again
        job = _job(config, show_output=True)

        _capture(job, lambda job: job.begin())
        (done, fail), command_list, output = _capture(job, lambda job: job.end(), output="all good\n")

        assert_not_in("all good", output)


####################################################################################################################################
def test_job_end_fail():
    """A test that failed is reported with what it wrote, whatever its exit status was."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path)
        job = _job(config, _run(vm="u24"))

        _capture(job, lambda job: job.begin())
        (done, fail), command_list, output = _capture(job, lambda job: job.end(), status=25, output="it broke\n")

        assert_true(done)
        assert_true(fail)
        assert_equal(output.split("\n")[0], "P00  ERROR: P1-T1/1 - vm=u24, module=common/error (err25):")
        assert_in("it broke", output)

        # Nothing is written where coverage would go since the test did not finish, and the container is removed
        assert_equal(
            command_list,
            ["remove ^test-0($|-)", _expect("chmod -R 700 [PATH]/test-0/* 2>&1;rm -rf [PATH]/test-0", config)],
        )

        # Anything on stderr is a failure whatever the exit status was, e.g. a valgrind error the binary did not report
        job = _job(config)

        _capture(job, lambda job: job.begin())
        (done, fail), command_list, output = _capture(job, lambda job: job.end(), error="valgrind said no\n")

        assert_true(fail)

        # A test that wrote nothing at all still reports something
        assert_in("NO OUTPUT ON STDOUT OR STDERR", output)


####################################################################################################################################
def test_job_profile():
    """A profile run writes the profile the test generated."""

    with tempfile.TemporaryDirectory() as path:
        config = Config(path, profile=True)
        job = _job(config, _run(vm="u24"))

        _capture(job, lambda job: job.begin())

        # The object directory meson compiled the test into is found under either name it has had
        file_write(os.path.join(path, "unit-0/u24/build/test-unit.p/test.c.gcda"), "")

        (done, fail), command_list, output = _capture(job, lambda job: job.end())

        assert_equal(
            command_list[:2],
            [
                _expect(
                    "docker exec -i -u [USER] test-0 gprof [PATH]/unit-0/u24/build/test-unit"
                    " [PATH]/unit-0/u24/build/gmon.out > [PATH]/unit-0/u24/gprof.txt",
                    config,
                ),
                _expect("cp [PATH]/unit-0/u24/gprof.txt [PATH]/repo/test/result/profile/gprof.txt", config),
            ],
        )


####################################################################################################################################
def test_job_retry():
    """A test that failed is started again until it runs out of retries."""

    with tempfile.TemporaryDirectory() as path:
        job = _job(Config(path, retry=1))

        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_true(started)
        assert_in(" --log-level=info ", job.exec.command)

        # The last try runs at a higher log level so there is something to look at when it fails again
        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_true(started)
        assert_in(" --log-level=debug ", job.exec.command)
        assert_equal(job.description, "P1-T1/1 - vm=none, module=common/error (retry 1)")

        # There are no tries left so the driver is told to give up on it
        started, command_list, output = _capture(job, lambda job: job.begin())

        assert_false(started)
        assert_equal(output, "")
