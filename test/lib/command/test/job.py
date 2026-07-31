"""Test Job.

Runs one test and monitors its progress. The test runs in another process, and in a container of its own when the vm is not none,
so several tests can run at once. The harness is invoked there with the unit command, which builds the test and runs it.

The job is polled rather than waited on so the driver can keep every vm busy, which is why begin() only starts the test and end()
reports whether it has finished."""

####################################################################################################################################
import math
import os
import time

from command.test.container import container_remove
from command.test.define import TEST_LANG_C, TEST_TYPE_PERFORMANCE
from common.exec import Exec, exec_one
from common.log import *
from common.storage import path_create
from common.user import user_name
from common.vm import *


####################################################################################################################################
class TestJob:
    """Run a test and monitor its progress."""

    def __init__(self, config, run, vm_idx, vm_max, test_idx, test_max, image, show_output):
        self.config = config
        self.run = run
        self.vm_idx = vm_idx
        self.vm_max = vm_max
        self.test_idx = test_idx
        self.test_max = test_max
        self.image = image  # Container image the test runs in
        self.show_output = show_output  # Write the test output as it arrives?

        self.try_idx = 0
        self.log_level = config.log_level
        self.coverage = False
        self.exec = None
        self.description = None
        self.time_begin = None

        # A unit test is built for the vm it runs on but an integration test runs the binary that was built for none
        self.path_unit = os.path.join(config.test_path, "unit-%u" % vm_idx, VM_NONE if run.integration else run.vm)
        self.path_data = os.path.join(config.test_path, "data-%u" % vm_idx)
        self.path_repo = os.path.join(config.test_path, "repo")
        self.path_host = os.path.join(config.test_path, "test-%u" % vm_idx)
        self.container = "test-%u" % vm_idx

    ################################################################################################################################
    def _exec_container(self, command):
        """Run a command in the test container when the test has one."""

        if self.run.vm == VM_NONE:
            return command

        return "docker exec -i -u %s %s %s" % (user_name(), self.container, command)

    ################################################################################################################################
    def _container_run(self):
        """Start the container the test runs in."""

        config = self.config
        path_build = os.path.join(config.test_path, "build", self.run.vm)
        path_ccache = os.path.join(config.test_path, "ccache")

        # An integration test starts its own containers so it never gets here, which means the unit build and the ccache that is
        # shared with the host and every other container are always mounted
        mount = " -v %s:%s -v %s:%s -v %s:%s:ro -v %s:%s -e CCACHE_DIR=%s" % (
            self.path_unit,
            self.path_unit,
            self.path_data,
            self.path_data,
            path_build,
            path_build,
            path_ccache,
            path_ccache,
            path_ccache,
        )

        exec_one(
            "docker run%s -itd -h %s-test --name=%s -v %s:%s%s -v %s:%s -v %s:%s %s"
            % (
                "" if config.vm_arch is None else " --platform linux/%s" % config.vm_arch,
                self.run.vm,
                self.container,
                self.path_host,
                self.path_host,
                mount,
                config.repo_path,
                config.repo_path,
                self.path_repo,
                self.path_repo,
                self.image,
            )
        )

    ################################################################################################################################
    def _command(self):
        """Build the command that runs the test."""

        config = self.config
        run = self.run

        # An integration test runs the binary that was built for none rather than one built for the vm
        vm = VM_NONE if run.integration else run.vm

        command = (
            "python3 %s/test/test.py unit --repo-path=%s --test-path=%s --log-level=%s --log-level-test=%s --vm=%s"
            % (
                self.path_repo,
                self.path_repo,
                config.test_path,
                LEVEL_NAME[self.log_level],
                LEVEL_NAME[config.log_level_test],
                run.vm,
            )
            + ("" if config.vm_arch is None else " --vm-arch=%s" % config.vm_arch)
            + " --vm-id=%u" % self.vm_idx
            + (" --profile" if config.profile else "")
            + "".join(" --test=%u" % test for test in run.test_list or [])
            + ("" if config.log_timestamp else " --no-log-timestamp")
            + ("" if config.tz is None else " --tz='%s'" % config.tz)
            + " --scale=%u" % config.scale
            + ("" if run.pg_version is None else " --pg-version=%s" % run.pg_version)
            + ("" if config.back_trace else " --no-back-trace")
            + ("" if self.coverage else " --no-coverage")
        )

        # A python test writes its own coverage since there is no gcov step for it in end()
        if run.module.lang != TEST_LANG_C and self.coverage:
            command += " --coverage-file=%s/test/result/coverage/raw/%s.json" % (config.repo_path, run.coverage_name)

        command += " " + run.module.name

        # A python test is run by the harness above so there is no binary to run here
        if run.module.lang == TEST_LANG_C:
            valgrind = ""

            if config.valgrind and not run.performance:
                path_suppress = os.path.join(self.path_repo, "test/src/valgrind.suppress.%s" % vm)

                valgrind = (
                    "valgrind -q --gen-suppressions=all"
                    + (" --suppressions=%s" % path_suppress if os.path.exists(path_suppress) else "")
                    + " --exit-on-first-error=yes --leak-check=full --leak-resolution=high --error-exitcode=25 "
                )

            # Copy stderr to both stderr and stdout so it is displayed and detected as an error. Piping to tee would mask the test
            # exit status (the pipe returns tee's status) so save the status to a file and exit with it explicitly. Otherwise a
            # test failure (e.g. a valgrind error) would be hidden and reported as missing coverage instead.
            command += (
                " && \\\nexec 3>&1 && \\\n{ %s%s/build/test-unit 2>&1 1>&3; echo $? > %s/result; } | tee /dev/stderr && \\\n"
                % (valgrind, self.path_unit, self.path_unit)
                + "exit $(cat %s/result)" % self.path_unit
            )

        # Run in the container when the test has one
        if vm != VM_NONE:
            command = "docker exec -i -u %s %s bash -l -c '\\\n%s'" % (user_name(), self.container, command)

        return command

    ################################################################################################################################
    def begin(self):
        """Start the test and report whether it was started.

        A test that has used up its retries is not started again, which is how the driver knows to give up on it."""

        config = self.config
        run = self.run

        self.time_begin = time.time()
        self.try_idx += 1

        if self.try_idx > config.retry + 1:
            return False

        # Raise the log level for the last try so there is something to look at when it fails again
        if self.try_idx != 1 and self.try_idx == config.retry + 1:
            self.log_level = DEBUG

        self.description = (
            "P%0*d-T%0*d/%0*d - vm=%s, module=%s"
            % (
                len(str(self.vm_max)),
                self.vm_idx + 1,
                len(str(self.test_max)),
                self.test_idx + 1,
                len(str(self.test_max)),
                self.test_max,
                run.vm,
                run.module.name,
            )
            + ("" if run.test_list is None else ", test=%s" % ",".join(str(test) for test in sorted(run.test_list)))
            + ("" if run.pg_version is None else ", pg-version=%s" % run.pg_version)
            + ("" if self.try_idx == 1 else " (retry %u)" % (self.try_idx - 1))
        )

        # A dry run lists the tests it would run, so the list goes to the log rather than the detail the tests write
        dry_run = config.dry_run and not config.vm_out

        # A blank line separates the test from the output that follows it, which is only shown when it was asked for
        separator = "\n" if (not config.dry_run and config.vm_out) or self.show_output else ""

        log(INFO if dry_run or self.show_output else DETAIL, self.description + separator)

        if dry_run:
            return False

        path_create(self.path_host, mode=0o770)

        if not run.integration:
            path_create(self.path_unit, mode=0o770)

            # A performance test is timed rather than checked so it writes no data
            if run.module.type != TEST_TYPE_PERFORMANCE:
                path_create(self.path_data, mode=0o770)

            if run.vm != VM_NONE:
                self._container_run()

        # Coverage is collected when the vm supports it, except for a performance or profile run where it would skew the timing
        self.coverage = vm_get(run.vm).coverage_c and config.coverage and not run.performance and not config.profile

        self.exec = Exec(self._command(), show_output=self.show_output)
        self.exec.begin()

        return True

    ################################################################################################################################
    def _profile(self):
        """Write the profile the test generated."""

        path_profile = os.path.join(self.config.repo_path, "test/result/profile")

        exec_one(
            self._exec_container(
                "gprof %s/build/test-unit %s/build/gmon.out > %s/gprof.txt" % (self.path_unit, self.path_unit, self.path_unit)
            )
        )

        path_create(path_profile)
        exec_one("cp %s/gprof.txt %s/gprof.txt" % (self.path_unit, path_profile))

    ################################################################################################################################
    def _coverage(self):
        """Write the coverage the test generated.

        Meson names the directory it compiled the test into and the name has changed between versions, so look for both."""

        path_object = os.path.join(self.path_unit, "build/test-unit.p")

        if not os.path.exists(path_object):
            path_object = os.path.join(self.path_unit, "build/test-unit@exe")

        exec_one(
            self._exec_container(
                "gcov --json-format --stdout --branch-probabilities %s/test.c.gcda > %s/test/result/coverage/raw/%s.json"
                % (path_object, self.config.repo_path, self.run.coverage_name)
            )
        )

    ################################################################################################################################
    def end(self):
        """Report whether the test has finished and whether it failed.

        The test is waited on when it is the only one running, since there is nothing else to do until it is done."""

        config = self.config
        status = self.exec.end(wait=self.vm_max == 1)

        if status is None:
            return False, False

        if self.show_output:
            print("")

        # Write the profile and coverage a C test left behind. A python test wrote its own coverage while it ran.
        #
        # The cleanup below runs whatever happens here, since a test leaves files behind that only the user it ran as can remove and
        # this is the last chance to remove them. Left behind, they make every later run fail on a test path that cannot be cleaned.
        try:
            if status == 0 and self.run.module.lang == TEST_LANG_C and not self.run.integration:
                if config.profile:
                    self._profile()

                if self.coverage:
                    self._coverage()
        finally:
            self._cleanup()

        elapsed = math.ceil((time.time() - self.time_begin) * 100) / 100

        # Anything written to stderr is a failure whatever the exit status was, e.g. a valgrind error
        fail = status != 0 or self.exec.error != ""

        if fail:
            output = self.exec.output.strip()

            log(
                ERROR,
                self.description
                + " (err%d%s)" % (status, "-%.2fs" % elapsed if config.log_timestamp else "")
                + ("" if self.show_output else ":\n\n%s\n" % (output if output else "NO OUTPUT ON STDOUT OR STDERR")),
            )
        else:
            log(
                INFO,
                self.description
                + (" (%.2fs)" % elapsed if config.log_timestamp else "")
                + (":\n\n%s\n" % self.exec.output.strip() if config.vm_out and not self.show_output else ""),
            )

        return True, fail

    ################################################################################################################################
    def _cleanup(self):
        """Remove the containers the test ran in and the path it ran in."""

        if not self.config.cleanup:
            return

        # An integration test starts its own containers and names them after the job, so those are removed here too. Anything still
        # running in one would keep writing to the test path while it is being removed.
        if self.run.vm != VM_NONE:
            container_remove("^%s($|-)" % self.container)

        exec_one("chmod -R 700 %s/* 2>&1;rm -rf %s" % (self.path_host, self.path_host))
