"""Test Command.

Selects the tests to run, generates the code they need, builds the pgbackrest binary when a test uses it, and runs the tests. Each
test runs in a separate process (and its own container when the vm is not none) so several can run at once.

This is what a developer runs, which is why it needs no command. The unit and coverage commands are the steps it drives, and
they are commands of their own because a test runs inside a container where only the repository copy is available."""

####################################################################################################################################
import os
import re
import time

from command.test.container import container_build, container_remove, container_repo
from command.test.define import test_def_parse
from command.test.job import TestJob
from command.test.list import test_list_get
from common.error import ToolError, check
from common.exec import Exec, exec_one
from common.log import *
from common.storage import file_read, file_write, file_write_differs, path_create
from common.user import user_name
from common.vm import *
from config.project import project_version, project_version_part


####################################################################################################################################
def _version_update(path_repo):
    """Update the version in src/version.h and meson.build.

    The version components in src/version.h are the source of truth, so the version string, the version number, and the version
    meson builds with are all generated from them rather than being kept in sync by hand."""

    path_version = os.path.join(path_repo, "src/version.h")
    part = project_version_part(path_repo)

    version = project_version(path_repo)
    version_num = "%d%03d%03d" % (int(part["MAJOR"]), int(part["MINOR"]), int(part["PATCH"]))

    # Version defines in src/version.h
    result = ""

    for line in file_read(path_version).rstrip("\n").split("\n"):
        if line.startswith("#define PROJECT_VERSION "):
            line = "#define PROJECT_VERSION" + " " * 45 + '"%s"' % version
        elif line.startswith("#define PROJECT_VERSION_NUM "):
            line = "#define PROJECT_VERSION_NUM" + " " * 41 + version_num

        result += line + "\n"

    file_write_differs(path_version, result)

    # Version meson builds with
    path_meson = os.path.join(path_repo, "meson.build")
    result = ""

    for line in file_read(path_meson).rstrip("\n").split("\n"):
        if line.startswith("    version: '"):
            line = "    version: '%s'," % version

        result += line + "\n"

    file_write_differs(path_meson, result)

    return version


####################################################################################################################################
def _repo_copy(config):
    """Copy the repository so a test can tell which files it changed.

    Only version controlled files are copied, and the generated files that the code generation below writes into the copy are left
    out so a stale copy of them cannot be used."""

    exec_one(
        "git -C %s ls-files -c --others --exclude-standard |" % config.repo_path
        + " rsync -rLtW --delete --files-from=- --exclude=test/result"
        + " --exclude=src/postgres/interface.auto.c.inc --exclude=src/command/help/help.auto.c.inc"
        # This option is not supported on MacOS. The eventual plan is to remove the need for it.
        + ("" if exec_one("uname").strip() == "Darwin" else " --ignore-missing-args")
        + " %s/ %s" % (config.repo_path, os.path.join(config.test_path, "repo"))
    )


####################################################################################################################################
def _code_generate(config, path_repo_copy):
    """Generate the code that is built from the declarations in the source.

    This runs on the host rather than in a container, since the generator is python and needs nothing built first."""

    path_repo = config.repo_path
    command = ""

    log(INFO, "autogenerate code")

    # A dry run does the minimum required, i.e. only what building the test list depends on
    if not config.dry_run:
        for generate in ("config", "error", "postgres-version"):
            command += "%s/build/build.py %s && \\\n" % (path_repo, generate)

    # The PostgreSQL interfaces are generated into the repository copy, since they are built rather than committed
    command += "%s/build/build.py postgres --build-path=%s" % (path_repo, path_repo_copy)

    exec_one(command)


####################################################################################################################################
def _build(config, bin_required):
    """Build the pgbackrest binary when a test needs it.

    A unit test is built from the repository copy by the unit command so there is nothing to build for it here, but its build path
    is mounted into the container so the path must exist either way."""

    path_build = os.path.join(config.test_path, "build", config.vm)
    build_clean = not os.path.exists(os.path.join(path_build, "build.ninja"))

    log(INFO, ("clean " if build_clean else "") + "build for %s (%s)" % (config.vm, path_build))

    command = "ninja -C %s src/pgbackrest 2>&1" % path_build if bin_required else None

    if build_clean:
        command = "meson setup -Dwerror=true -Dfatal-errors=true -Dbuildtype=debug %s %s" % (path_build, config.repo_path) + (
            "" if command is None else " && \\\n" + command
        )

    # There is nothing to do when the build is already set up and no binary is needed
    if command is None:
        return

    if config.vm != VM_NONE:
        command = "docker exec -i -u %s test-build bash -c '%s'" % (user_name(), command)

    exec_one(command, show_output=config.log_level >= DETAIL)


####################################################################################################################################
def _run(config, test_list, image):
    """Run the tests, keeping every vm busy, and report how many failed and how many were retried."""

    process_list = [None] * config.vm_max
    show_output = config.vm_out and (len(test_list) == 1 or config.vm_max == 1) and not config.dry_run
    count_fail = 0
    count_retry = 0
    idx_test = 0

    while True:
        # Wait for a vm to free up
        while True:
            active = 0

            for idx in range(config.vm_max):
                job = process_list[idx]

                if job is None:
                    continue

                done, fail = job.end()

                if not done:
                    active += 1

                    continue

                process_list[idx] = None

                if not fail:
                    continue

                # Start the test again when it has a retry left, else count it as a failure
                if job.begin():
                    process_list[idx] = job
                    count_retry += 1
                    active += 1
                else:
                    count_fail += 1

            # Only wait when every vm is busy or every test has been assigned, otherwise there is something to do
            if active == config.vm_max or idx_test == len(test_list):
                time.sleep(0.05)

            if active != config.vm_max:
                break

        # Assign tests to the vms that are free
        for idx in range(config.vm_max):
            if process_list[idx] is not None or idx_test == len(test_list):
                continue

            job = TestJob(config, test_list[idx_test], idx, config.vm_max, idx_test, len(test_list), image, show_output)
            idx_test += 1
            active += 1

            if job.begin():
                process_list[idx] = job

        if active == 0:
            break

    return count_fail, count_retry


####################################################################################################################################
def _coverage(config, test_list):
    """Merge the coverage the tests produced and write the report, reporting whether any module is missing coverage."""

    command = Exec(
        "python3 %s/test/test.py coverage --log-level=warn --vm=%s --repo-path=%s --test-path=%s%s %s"
        % (
            config.repo_path,
            config.vm,
            config.repo_path,
            config.test_path,
            " --coverage-summary" if config.coverage_summary else "",
            " ".join(run.module.name for run in test_list),
        ),
        show_output=True,
    )
    command.begin()
    status = command.end()

    # The coverage command reports incomplete coverage with a status of one, which is not an error here since the tests all passed
    if status > 1:
        raise ToolError("coverage command failed", status)

    if status == 0:
        log(INFO, "tested modules have full coverage")
    # Show where the report is so it can be pasted into a browser to see what is missing
    else:
        log(INFO, "coverage report written to file://%s" % os.path.join(config.repo_path, "test/result/coverage/coverage.html"))

    return status == 1


####################################################################################################################################
def cmd_test(config):
    """Run the tests."""

    time_begin = time.time()
    path_repo = config.repo_path
    path_repo_copy = os.path.join(config.test_path, "repo")

    # Set a neutral umask so tests work as expected
    os.umask(0)

    log(INFO, "test begin on %s - log level %s" % (host_arch(), LEVEL_NAME[config.log_level]))

    # The coverage summary is generated from the C tests that provide coverage, so it selects them
    if config.coverage_summary:
        config.coverage_only = True
        config.c_only = True

    # Profiling needs a build without the instrumentation that would skew the timing
    if config.profile:
        config.back_trace = False
        config.valgrind = False
        config.coverage = False

    # Check the options that the parser cannot
    check(len(config.run) <= 1, "only one --run can be provided")
    check(not config.test or len(config.module) == 1, "only one --module can be provided when --test is specified")
    check(not config.run or len(config.test) == 1, "only one --test can be provided when --run is specified")
    check(not config.vm_build or config.vm != VM_NONE, "select a vm to build, or all of them")

    # Every vm can be built at once but a test runs on one vm, since it needs the binary and the build for the vm it runs on
    check(config.vm != VM_ALL or config.vm_build, "select a single vm to test on")

    # Check the vm now so a typo is reported before anything is generated or built
    if config.vm != VM_ALL:
        vm_get(config.vm)

    # The test path holds everything the tests generate, which would be a mess to sort out from the repository
    check(
        not (config.test_path + "/").startswith(path_repo + "/"),
        "test path '%s' may not be in the repo path '%s'\n" % (config.test_path, path_repo)
        + "HINT: was test.py run in '%s'?\n" % path_repo
        + "HINT: use --test-path to set a test path\n"
        + "HINT: run test.py from outside the repo, e.g. 'pgbackrest/test/test.py'",
    )

    # Clean working and result paths
    if config.clean or config.clean_only:
        log(INFO, "clean working (%s) and result (%s/test/result) paths" % (config.test_path, path_repo))

        for path in (config.test_path, os.path.join(path_repo, "test/result")):
            if os.path.exists(path):
                try:
                    exec_one("find %s -mindepth 1 -print0 | xargs -0 rm -rf" % path)
                except ToolError as error:
                    # A test that did not get to clean up after itself can leave files owned by root behind, which is the only thing
                    # here that cannot be removed as the user the tests run as
                    raise ToolError("%s\nHINT: a test may have left files owned by root, so try 'sudo rm -rf %s/*'" % (error, path))

        if config.clean_only:
            return 0

    # Build the containers
    if config.vm_build:
        container_build(config)

        return 0

    # Load the test definitions
    module_list = test_def_parse(path_repo)

    # Clean up data left by the prior run
    coverage_c = vm_get(config.vm).coverage_c

    if not config.dry_run or config.vm_out:
        log(INFO, "cleanup old data" + (" and containers" if config.vm != VM_NONE else ""))

        if config.vm != VM_NONE:
            container_remove("test-([0-9]+|build)")

        exec_one(
            "chmod 700 -R %s/test-* 2>&1 || true && rm -rf %s/temp %s/test-* %s/data-*"
            % (config.test_path, config.test_path, config.test_path, config.test_path)
        )
        path_create(os.path.join(config.test_path, "temp"), mode=0o770)

        # Overwrite the C coverage report so it will load but not show old coverage
        path_create(os.path.join(path_repo, "test/result/coverage"), mode=0o770)
        file_write(
            os.path.join(path_repo, "test/result/coverage/coverage.html"),
            "<center>[ %s ]</center>" % ("Generating Coverage Report" if config.coverage else "No Coverage Testing"),
        )

        # Clear the coverage the prior run produced
        if coverage_c and not config.dry_run:
            exec_one("rm -rf %s/test/result/coverage/raw/*" % path_repo)
            path_create(os.path.join(path_repo, "test/result/coverage/raw"), mode=0o770)

    _version_update(path_repo)

    # A single ccache is shared by the host and all containers so a file compiled in one is a hit in the others, e.g. a module
    # compiled for one unit test is a hit for every other unit test that includes it. ccache is safe for concurrent access so there
    # is no need to keep a separate cache per vm or vm index. Keep the cache in the test path so it is removed by --clean.
    path_ccache = os.path.join(config.test_path, "ccache")
    path_create(path_ccache, mode=0o770)

    # Set for builds that run on the host. Containers are passed the path explicitly since docker does not pass the environment.
    os.environ["CCACHE_DIR"] = path_ccache

    # Start the build container when the tests run in one
    image = "%s:%s-test-%s" % (container_repo(), config.vm, config.vm_arch if config.vm_arch is not None else host_arch())

    if config.vm != VM_NONE:
        # The cache does not need to be mounted since it is in the test path, which is mounted below
        exec_one(
            "docker run%s -itd -h test-build --name=test-build -v %s:%s -v %s:%s -e CCACHE_DIR=%s %s"
            % (
                "" if config.vm_arch is None else " --platform linux/%s" % config.vm_arch,
                path_repo,
                path_repo,
                config.test_path,
                config.test_path,
                path_ccache,
                image,
            )
        )

    # Create the repository copy path first so an obvious rsync error is reported rather than a confusing one
    path_create(path_repo_copy, mode=0o770)

    _code_generate(config, path_repo_copy)

    if config.gen_only:
        return 0

    _repo_copy(config)

    # Determine which tests to run and what they need built
    test_list = []
    bin_required = config.build_only
    unit_required = config.build_only

    if not config.build_only:
        test_list = test_list_get(module_list, config)

        # The binary is required for an integration test and for any test that asks for it. The build path is mounted into the
        # container of every other test, so it must exist even when nothing is built there.
        bin_required = any(run.integration or run.module.bin_required for run in test_list)
        unit_required = any(not run.integration for run in test_list)

    if not config.dry_run:
        if bin_required or unit_required:
            _build(config, bin_required)

        # Shut down the build container
        if config.vm != VM_NONE:
            exec_one("docker rm -f test-build")

        if config.build_only:
            return 0

    if not test_list:
        raise ToolError("no tests were selected")

    log(INFO, "%u test%s selected\n" % (len(test_list), "" if len(test_list) == 1 else "s"))

    # Preserving the results of more than one test is not possible since they share a path
    check(config.cleanup or len(test_list) == 1, "--no-cleanup is not valid when more than one test will run")

    # Only use one vm for a dry run so the tests are listed in order
    if config.dry_run:
        config.vm_max = 1

    count_fail, count_retry = _run(config, test_list, image)

    # Write the coverage report. There is nothing to report when a single run was selected since that cannot cover a module.
    uncovered = False

    if coverage_c and config.coverage and not config.dry_run and count_fail == 0 and not config.run:
        uncovered = _coverage(config, test_list)

    log(
        INFO,
        ("DRY RUN COMPLETED" if config.dry_run else "TESTS COMPLETED")
        + (
            " SUCCESSFULLY" + (" WITH MODULE(S) MISSING COVERAGE" if uncovered else "")
            if count_fail == 0
            else " WITH %u FAILURE(S)" % count_fail
        )
        + ("" if count_retry == 0 else ", %u RETRY(IES)" % count_retry)
        + ("" if not config.log_timestamp else " (%us)" % int(time.time() - time_begin)),
    )

    return 1 if count_fail > 0 or (uncovered and not config.coverage_summary) else 0
