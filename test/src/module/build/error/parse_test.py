"""Test Error Parse."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_write
from error.parse import *


####################################################################################################################################
def _parse(error):
    """Parse an error declaration."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/error.yaml"), error)

        return bld_err_parse(path)


####################################################################################################################################
def _error(error):
    """Parse an error declaration that is expected to fail and return the message."""

    with assert_raises(ToolError) as exception:
        _parse(error)

    return str(exception.exception)


####################################################################################################################################
def test_error_parse():
    """An error is a code on its own or a code with the attributes that go with it."""

    err_list = _parse("assert:\n  code: 25\n  fatal: true\nchecksum: 26\nfile-open: 41\n")

    # Errors keep the order they were declared in, since the generated array is indexed by nothing but position
    assert_equal(
        [(err.name, err.code, err.fatal) for err in err_list],
        [("assert", 25, True), ("checksum", 26, False), ("file-open", 41, False)],
    )


####################################################################################################################################
def test_error_parse_error():
    """An error declaration that cannot be honored is reported."""

    assert_equal(_error("bogus:\n  bogus: 1\n"), "unknown error definition 'bogus'")
    assert_equal(_error("bogus:\n  fatal: true\n"), "error 'bogus' requires a code")
    assert_equal(_error("bogus: x\n"), "error 'bogus' code 'x' is not an integer")

    # A code must be inside the range the C reserves for the errors declared here
    assert_equal(_error("bogus: 24\n"), "error 'bogus' code must be >= 25 and <= 125")
    assert_equal(_error("bogus: 126\n"), "error 'bogus' code must be >= 25 and <= 125")
