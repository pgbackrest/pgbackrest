"""Test Project Facts."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_write
from config.project import *

VERSION = "2.60.0dev"

# The version header as the project writes it. The parts are what a release edits and the version string beside them is generated
# from them by a test run, so the string here is deliberately stale to show which of the two is read.
VERSION_H = """#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       60
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"

#define PROJECT_VERSION                                             "0.0.0"
"""


####################################################################################################################################
def test_project_version():
    """The version is built from the parts rather than read from the string generated from them."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "src/version.h"), VERSION_H)

        assert_equal(project_version(path), VERSION)
        assert_equal(project_version_part(path)["SUFFIX"], '"dev"')

        # A release clears the development marker, and the version is that version from then on, whatever the generated string says
        file_write(os.path.join(path, "src/version.h"), VERSION_H.replace('"dev"', '""'))

        assert_equal(project_version(path), "2.60.0")


####################################################################################################################################
def test_project_version_error():
    """A version header missing a part is an error rather than a version built from what is left."""

    with tempfile.TemporaryDirectory() as path:
        # The define must be at the start of a line so a mention of it in a comment is not read as the version
        file_write(os.path.join(path, "src/version.h"), VERSION_H.replace("#define PROJECT_VERSION_PATCH", "// PATCH is"))

        with assert_raises(ToolError) as error:
            project_version(path)

        assert_equal(str(error.exception), "unable to find PROJECT_VERSION_PATCH in src/version.h")
