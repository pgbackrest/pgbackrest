"""Test Configuration Load."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.log import *
from config.config import *

# The parts of the version, which is what the version is built from
VERSION = "2.60.0dev"
VERSION_H = """#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       60
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      "dev"
"""


####################################################################################################################################
def _version_write(path):
    """Write the version header, which is the only part of the repository the config load reads."""

    os.mkdir(os.path.join(path, "src"))

    with open(os.path.join(path, "src/version.h"), "w") as file:
        file.write(VERSION_H)


####################################################################################################################################
def _cfg_load(arg_list, path):
    """Load a configuration and put the log settings back, since loading sets them for the whole harness."""

    try:
        return cfg_load(arg_list, path)
    finally:
        log_init(INFO, True)


####################################################################################################################################
def test_config_load():
    """A command line is parsed and what cannot be expressed in the parser is applied."""

    with tempfile.TemporaryDirectory() as path:
        _version_write(path)

        config = _cfg_load(
            ["unit", "common/error", "--repo-path=%s" % path, "--test-path=%s/test" % path, "--log-level=detail"], path
        )

        assert_equal(config.command, "unit")
        assert_equal(config.repo_path, path)
        assert_equal(config.test_path, os.path.join(path, "test"))

        # Levels are ids by the time the load is done, since the parser has already checked that the names are valid
        assert_equal(config.log_level, DETAIL)
        assert_equal(config.log_level_test, OFF)

        # Paths are normalized so the harness does not depend on the working directory
        config = _cfg_load(["unit", "common/error", "--repo-path=%s/." % path, "--test-path=%s/test/.." % path], path)

        assert_equal(config.repo_path, path)
        assert_equal(config.test_path, path)


####################################################################################################################################
def test_config_load_test():
    """A run with no command runs the tests, on the repository it is part of rather than on the copy they are built from."""

    with tempfile.TemporaryDirectory() as path:
        _version_write(path)

        config = _cfg_load(["--test-path=%s/test" % path], path)

        assert_equal(config.command, "test")
        assert_equal(config.repo_path, path)
        assert_equal(config.log_level, INFO)

        # Quiet is a shorthand for turning the log off, which is how the documentation build runs the tests
        assert_equal(_cfg_load(["--test-path=%s/test" % path, "--quiet"], path).log_level, OFF)


####################################################################################################################################
def test_config_load_error():
    """A repository that is not there is reported here rather than looking like a run that found nothing."""

    with tempfile.TemporaryDirectory() as path:
        _version_write(path)

        path_missing = os.path.join(path, "missing")

        with assert_raises(ToolError) as error:
            _cfg_load(["unit", "common/error", "--repo-path=%s" % path_missing], path)

        assert_equal(str(error.exception), "repo path '%s' does not exist" % path_missing)
