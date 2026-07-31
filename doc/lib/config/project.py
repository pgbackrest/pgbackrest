"""Project Facts.

What the project calls itself and what version it is, read from the source rather than said again here so there is one place a
release changes. Both the documentation and the test harness need them, which is why they are not with either one."""

####################################################################################################################################
import os
import re

from common.error import ToolError
from common.storage import file_read

# Name of the project executable, which is also the name of the repository and of the container registry namespace
PROJECT_EXE = "pgbackrest"

# Name of the project as it is written
PROJECT_NAME = "pgBackRest"


####################################################################################################################################
def project_version_part(path_repo):
    """The parts the version is made of, read from src/version.h in the repository this script was checked out into."""

    content = file_read(os.path.join(path_repo, "src/version.h"))
    result = {}

    for name in ("MAJOR", "MINOR", "PATCH", "SUFFIX"):
        match = re.search(r"^#define PROJECT_VERSION_%s\s+(\S+)$" % name, content, re.M)

        if match is None:
            raise ToolError("unable to find PROJECT_VERSION_%s in src/version.h" % name)

        result[name] = match.group(1)

    return result


####################################################################################################################################
def project_version(path_repo):
    """The version the project is.

    Built from the parts rather than read from the version string beside them, since the parts are what a release edits and the
    string is generated from them by a test run. Reading the string would report the version as of the last test run, which during a
    release is the version before it."""

    part = project_version_part(path_repo)

    return "%s.%s.%s%s" % (part["MAJOR"], part["MINOR"], part["PATCH"], part["SUFFIX"].strip('"'))
