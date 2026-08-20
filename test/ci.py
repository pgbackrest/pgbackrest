#!/usr/bin/env python3
"""pgBackRest CI Wrapper.

Prepares a CI runner and then runs the tests or builds the documentation on it. A separate program from the test harness because it
is about the runner rather than about the tests, and because it runs before the runner has what the harness needs.

This is the only place that knows what a runner is missing, since a runner starts from a stock image rather than from one of the
test images the tests themselves run in.

All output, including errors, goes to stdout so the run reads in the order it happened."""

####################################################################################################################################
import argparse
import os
import signal
import sys
import time

# Send everything written to stderr to stdout instead so the output is in the order it happened
sys.stderr = sys.stdout

# Do not cache bytecode. The harness runs from a copy of the repository that the linter then scans, so a __pycache__ written during
# import would be reported as an unexpected binary file. This must be set before the harness modules are imported below.
sys.dont_write_bytecode = True

# Each tool keeps its library beside itself and may use the libraries below it in the hierarchy, which for the harness is all of
# them. Insert them first, lowest last, so the harness modules are found before anything else on the path.
for lib in ("build", "doc", "test"):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), lib, "lib"))

from common.error import EXIT_ERROR, EXIT_TERM, ToolError, error_trace  # noqa: E402
from common.exec import exec_one  # noqa: E402
from common.log import *  # noqa: E402
from common.user import user_name  # noqa: E402
from common.vm import *  # noqa: E402
from config.project import PROJECT_EXE, project_version  # noqa: E402


####################################################################################################################################
def cfg_load(arg_list, path_repo):
    """Parse the command line and initialize logging."""

    parser = argparse.ArgumentParser(prog="ci.py", description="pgBackRest CI Wrapper")

    parser.add_argument("--version", action="version", version="pgBackRest %s CI Wrapper" % project_version(path_repo))
    parser.add_argument("target", choices=("doc", "test"), help="run the tests or build the documentation")
    parser.add_argument("--vm", default=VM_NONE, metavar="VM", help="vm to run on")
    parser.add_argument("--vm-arch", metavar="ARCH", help="vm architecture (defaults to the host architecture)")
    parser.add_argument("--distro", metavar="DISTRO", help="distribution to build the documentation for")
    parser.add_argument("--param", action="append", default=[], metavar="OPTION", help="option to pass to the test harness")
    parser.add_argument("--sudo", action="store_true", help="leave passwordless sudo in place for the run")
    parser.add_argument("--no-tempfs", dest="tempfs", action="store_false", help="do not mount a tmpfs for the test path")

    config = parser.parse_args(arg_list)
    config.repo_path = path_repo

    log_init(INFO, True)

    return config


####################################################################################################################################
def step(title, command_list, show_output=False):
    """Run the commands that make up one step of the run and report how long the step took."""

    time_begin = time.time()

    log(INFO, "Begin %s" % title)

    for command in command_list:
        log(INFO, "    Exec %s" % command)
        exec_one(command, show_output=show_output)

    log(INFO, "    End %s (%us)" % (title, int(time.time() - time_begin)))


####################################################################################################################################
def sudo_remove():
    """Remove passwordless sudo so a test cannot depend on having it.

    A runner grants it to every user, which the test containers do not."""

    step("remove sudo", ["sudo rm /etc/sudoers.d/%s" % user_name()])


####################################################################################################################################
def doc_build(config):
    """Build the documentation."""

    sudo_remove()

    step(
        "create link from home to repo for contributing doc",
        ["ln -s %s %s/%s" % (config.repo_path, os.environ["HOME"], PROJECT_EXE)],
    )

    distro = "" if config.distro is None else " --distro=%s" % config.distro

    step(
        "release documentation",
        ["%s/doc/release.py --build --no-gen%s" % (config.repo_path, distro)],
        show_output=True,
    )


####################################################################################################################################
def test_run(config):
    """Run the tests."""

    vm_arch = "" if config.vm_arch is None else " --vm-arch=%s" % config.vm_arch

    # Packages the tests need that the test images already have
    package = "gcc ccache git zlib1g-dev libssl-dev libxml2-dev libpq-dev pkg-config libssh2-1-dev valgrind"

    # Extra packages required when testing without containers
    if config.vm == VM_NONE:
        package += " liblz4-dev liblz4-tool zstd libzstd-dev bzip2 libbz2-dev"

    step(
        "/tmp/pgbackrest owned by root so tests cannot use it",
        ["sudo mkdir -p /tmp/%s && sudo chown root:root /tmp/%s && sudo chmod 700 /tmp/%s" % ((PROJECT_EXE,) * 3)],
    )

    step(
        "install test packages",
        ["sudo DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y %s" % package],
    )

    if not config.sudo:
        sudo_remove()

    # Build the containers
    if config.vm != VM_NONE:
        step(
            "%s build" % config.vm,
            ["%s/test/test.py vm-build --vm=%s%s" % (config.repo_path, config.vm, vm_arch)],
            show_output=True,
        )

    step(
        ("no container" if config.vm == VM_NONE else config.vm) + " test",
        [
            "%s/test/test.py --vm-max=2 --vm=%s%s%s"
            % (config.repo_path, config.vm, vm_arch, "".join(" --%s" % param for param in config.param))
        ],
        show_output=True,
    )


####################################################################################################################################
def command_run(config):
    """Prepare the runner and then run what was asked for on it."""

    # The package index is refreshed by the workflow rather than here. Keeping apt from waiting forever on a slow mirror means
    # rewriting the mirror list the runner image ships, which is about the runner rather than about the run.
    step(
        "install common packages",
        ["sudo DEBIAN_FRONTEND=noninteractive apt-get install -y meson python3-yaml"],
    )

    # The test path is a tmpfs so the tests are not limited by the runner's disk, which is slow and shared
    if config.tempfs:
        step(
            "mount tmpfs",
            ["mkdir -p -m 770 test", "sudo mount -t tmpfs -o size=2048m tmpfs test", "df -h test"],
            show_output=True,
        )

    if config.target == "doc":
        doc_build(config)
    else:
        test_run(config)

    log(INFO, "CI Complete")

    return 0


####################################################################################################################################
def main():
    """Main."""

    # Die silently on SIGPIPE as C programs do, rather than raising when output is piped to a command that exits early
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

    path_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    try:
        return command_run(cfg_load(sys.argv[1:], path_repo))
    except KeyboardInterrupt:
        # A ctrl-c is what was asked for, so report it the way the C reports a signal rather than as a stack trace
        log(ERROR, "terminated on signal SIGINT")

        return EXIT_TERM
    except ToolError as error:
        log(ERROR, error)

        return error.status
    except Exception as error:
        # An unexpected exception is a bug here rather than a problem with the run, so show the stack trace. The complete trace is
        # shown since a run cannot be repeated at debug level to get it, and there is no terminal here for it to get in the way of.
        log(ERROR, error_trace(error, True))

        return EXIT_ERROR


if __name__ == "__main__":
    sys.exit(main())
