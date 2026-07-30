"""Code Format Command.

Formats the source to the project standards, or checks that it is already formatted, which is what CI runs. C is formatted with
uncrustify and python with black, and the execute permissions are checked afterward since a stray execute bit is easy to introduce
and hard to notice.

Only version controlled files are formatted, which is what git reports here, so scratch work in the working tree is left alone."""

####################################################################################################################################
import os

from common.error import ToolError
from common.exec import exec_one
from common.log import *

# Line length black formats python to, which is the line length the rest of the project uses
_LINE_LENGTH = 132

# Files that are allowed to be executable. Everything else that is version controlled must not be, since a stray execute bit is
# usually the result of an editor or a file system that does not preserve modes.
_FILE_EXECUTABLE_LIST = (
    "build/build.py",
    "doc/doc.pl",
    "doc/doc.py",
    "doc/release.pl",
    "build/dist.sh",
    "test/ci.py",
    "test/smoke.py",
    "test/test.py",
)


####################################################################################################################################
def _file_list(path_repo):
    """The version controlled files, which are the ones that get formatted.

    Only regular files are returned. A file that is in the index but no longer in the working tree has nothing to format, and git
    reports a symlink to a directory as a single entry rather than descending into it."""

    file_list = exec_one("git -C %s ls-files -c --others --exclude-standard" % path_repo).splitlines()

    return [file for file in file_list if os.path.isfile(os.path.join(path_repo, file))]


####################################################################################################################################
def _format_c(path_repo, file_list, check):
    """Format the C with uncrustify."""

    command = "uncrustify -c %s/test/uncrustify.cfg%s" % (path_repo, " --check" if check else " --replace --no-backup")

    for file in file_list:
        # Skip anything that is not C
        if not file.endswith((".c", ".h", ".c.inc")):
            continue

        # Skip the test template, which does not format correctly, and vendorized and generated files
        if file == "test/src/test.c" or file.endswith((".vendor.h", ".vendor.c.inc", ".auto.c.inc")):
            continue

        command += " %s/%s" % (path_repo, file)

    exec_one(command)


####################################################################################################################################
def _format_python(path_repo, file_list, check):
    """Format the python with black."""

    command = "black --line-length=%u%s" % (_LINE_LENGTH, " --check --diff" if check else "")

    for file in file_list:
        if file.endswith(".py"):
            command += " %s/%s" % (path_repo, file)

    exec_one(command)


####################################################################################################################################
def _permission_check(path_repo, file_list):
    """Check that nothing is executable that is not meant to be, which formatting can munge."""

    for file in file_list:
        mode = os.stat(os.path.join(path_repo, file)).st_mode & 0o777

        if mode & 0o111 and file not in _FILE_EXECUTABLE_LIST:
            raise ToolError("expected mode '%04o' for '%s' but found '%04o'" % (mode & 0o666, file, mode))


####################################################################################################################################
def cmd_code_format(config):
    """Format the source, or check that it is already formatted."""

    path_repo = config.repo_path
    file_list = _file_list(path_repo)

    log(INFO, "code format" + (" check" if config.check else ""))

    try:
        _format_c(path_repo, file_list, config.check)
        _format_python(path_repo, file_list, config.check)
    except ToolError as error:
        if not config.check:
            raise

        # The check is what CI runs, so it says how to fix what it found rather than only what it found
        raise ToolError("%s\nHINT: run 'test.py code-format' to format the code" % error)

    _permission_check(path_repo, file_list)
