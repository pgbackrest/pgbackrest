"""Test Code Format Command.

The formatters are captured rather than run, since what this module does is decide which files to format with which formatter. The
repository is written here so a file can be left out of the working tree while git still reports it."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stdout
from unittest.mock import patch

from harness.test import *

from command.code.format import *
from common.error import *
from common.log import *
from common.storage import file_write, path_create

# What git reports for the repository, i.e. everything that is version controlled. src/gone.c is in the index but has been removed
# from the working tree and local is a path rather than a file, so neither is written as a file below.
FILE_LIST = (
    "README.md",
    "doc/doc.pl",
    "local",
    "src/build/help.auto.c.inc",
    "src/gone.c",
    "src/main.c",
    "src/main.h",
    "src/module.vendor.c.inc",
    "src/vendor.vendor.h",
    "test/lib/common/log.py",
    "test/src/test.c",
    "test/test.py",
)


####################################################################################################################################
class Config:
    """What the command reads from the command line."""

    def __init__(self, repo_path, check=False):
        self.repo_path = repo_path
        self.check = check


####################################################################################################################################
def _repo_create(path):
    """Write the repository, i.e. everything git reports that is a file that is still there."""

    for file in FILE_LIST:
        if file != "src/gone.c":
            file_write(os.path.join(path, file), "")

    # A path where git reported one, which is what it does for a symlink to a directory
    os.remove(os.path.join(path, "local"))
    path_create(os.path.join(path, "local"))

    return path


####################################################################################################################################
def _cmd_code_format(config, fail=None):
    """Run the command with the formatters captured rather than run.

    Returns the commands and the error raised, if any. Naming a formatter in fail makes it report that it found something."""

    command_list = []
    error = None

    def exec_fake(command, result_expect=0, show_output=False):
        command_list.append(command)

        if command.startswith("git "):
            return "\n".join(FILE_LIST) + "\n"

        if fail is not None and command.startswith(fail):
            raise ToolError("%s terminated unexpectedly [1]" % fail)

        return ""

    output = io.StringIO()
    log_init(INFO, False)

    try:
        with patch("command.code.format.exec_one", exec_fake):
            with redirect_stdout(output):
                try:
                    cmd_code_format(config)
                except ToolError as exception:
                    error = str(exception)
    finally:
        log_init(INFO, True)

    return command_list, error, output.getvalue()


####################################################################################################################################
def test_code_format():
    """Every version controlled file goes to the formatter for its language and nothing else does."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        command_list, error, output = _cmd_code_format(Config(path_repo))

        assert_is_none(error)
        assert_equal(output, "P00   INFO: code format\n")

        # Only the version controlled files are formatted, which is what git reports
        assert_equal(command_list[0], "git -C %s ls-files -c --others --exclude-standard" % path_repo)

        # The C, which is formatted in place
        assert_equal(
            command_list[1],
            "uncrustify -c %s/test/uncrustify.cfg --replace --no-backup %s/src/main.c %s/src/main.h" % ((path_repo,) * 3),
        )

        # The python, which is formatted to the same line length as the rest of the project
        assert_equal(
            command_list[2],
            "black --line-length=132 %s/test/lib/common/log.py %s/test/test.py" % ((path_repo,) * 2),
        )


####################################################################################################################################
def test_code_format_skip():
    """A file that is not formatted is left out of both formatters."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        command_list, error, output = _cmd_code_format(Config(path_repo))
        command = " ".join(command_list)

        # The test template does not format correctly since it is not valid C until the harness fills it in
        assert_not_in("test/src/test.c", command)

        # Nothing vendorized or generated, since neither is ours to format
        assert_not_in("help.auto.c.inc", command)
        assert_not_in("vendor.vendor.h", command)
        assert_not_in("module.vendor.c.inc", command)

        # Nothing in a language neither formatter handles
        assert_not_in("README.md", command)
        assert_not_in("doc.pl", command)

        # A file that git reports but is no longer in the working tree, since there is nothing there to format
        assert_not_in("gone.c", command)

        # An entry that is a path rather than a file, which is also not something to check the permissions of
        assert_not_in("local", command)


####################################################################################################################################
def test_code_format_check():
    """The check reports what is not formatted rather than formatting it, and says how to fix what it found."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        command_list, error, output = _cmd_code_format(Config(path_repo, check=True))

        assert_is_none(error)
        assert_equal(output, "P00   INFO: code format check\n")
        assert_in("uncrustify -c %s/test/uncrustify.cfg --check " % path_repo, command_list[1])

        # Black shows what it would change so the failure says what is wrong rather than only that something is
        assert_in("black --line-length=132 --check --diff ", command_list[2])

        # A check that finds something says how to fix it, since CI is where it usually runs
        for formatter in ("uncrustify", "black"):
            command_list, error, output = _cmd_code_format(Config(path_repo, check=True), fail=formatter)

            assert_equal(error, "%s terminated unexpectedly [1]\nHINT: run 'test.py code-format' to format the code" % formatter)

        # Formatting rather than checking reports what the formatter said and nothing else, since there is nothing to fix by hand
        command_list, error, output = _cmd_code_format(Config(path_repo), fail="uncrustify")

        assert_equal(error, "uncrustify terminated unexpectedly [1]")


####################################################################################################################################
def test_code_format_permission():
    """Nothing is executable that is not meant to be, which formatting can munge."""

    with tempfile.TemporaryDirectory() as path:
        path_repo = _repo_create(path)

        # The files that are meant to be executable are
        for file in ("doc/doc.pl", "test/test.py"):
            os.chmod(os.path.join(path_repo, file), 0o755)

        command_list, error, output = _cmd_code_format(Config(path_repo))

        assert_is_none(error)

        # Nothing else is
        os.chmod(os.path.join(path_repo, "src/main.c"), 0o750)

        command_list, error, output = _cmd_code_format(Config(path_repo))

        assert_equal(error, "expected mode '0640' for 'src/main.c' but found '0750'")
