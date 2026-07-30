"""Test Console Logging."""

####################################################################################################################################
import io
import re
from contextlib import redirect_stdout

from harness.test import *

from common.log import *


####################################################################################################################################
def _capture(function, level=INFO, timestamp=False):
    """Run a function with the log configured as specified and return what it wrote.

    Timestamps are off unless a test asks for them so the output is exact. The settings are module state shared by every test so
    they are put back afterwards."""

    output = io.StringIO()

    log_init(level, timestamp)

    try:
        with redirect_stdout(output):
            function()
    finally:
        log_init(INFO, True)

    return output.getvalue()


####################################################################################################################################
def test_log_level_parse():
    """A level name is converted to its id and an unknown name is not an error."""

    assert_equal(log_level_parse("off"), OFF)
    assert_equal(log_level_parse("error"), ERROR)
    assert_equal(log_level_parse("trace"), TRACE)

    # An unknown name is reported by returning nothing, since the caller has better context for the error
    assert_is_none(log_level_parse("bogus"))
    assert_is_none(log_level_parse(""))

    # Every level can be parsed back from the name it is logged with
    for level, name in LEVEL_NAME.items():
        assert_equal(log_level_parse(name), level, name)

    assert_equal(len(LEVEL_ID), len(LEVEL_NAME))

    # The values match LogLevel in src/common/logLevel.h, which the enum below is built from
    assert_equal((OFF, ERROR, WARN, INFO, DETAIL, DEBUG, TRACE), (0, 2, 3, 4, 5, 6, 7))


####################################################################################################################################
def test_log_level_enum():
    """The enum name matches LogLevel in src/common/logLevel.h, which the generated test.c is substituted with."""

    assert_equal(log_level_enum(OFF), "logLevelOff")
    assert_equal(log_level_enum(INFO), "logLevelInfo")
    assert_equal(log_level_enum(DETAIL), "logLevelDetail")


####################################################################################################################################
def test_log_format():
    """The level is right-aligned in the prefix and continuation lines are indented under it."""

    assert_equal(_capture(lambda: log(INFO, "message")), "P00   INFO: message\n")
    assert_equal(_capture(lambda: log(DETAIL, "message"), DETAIL), "P00 DETAIL: message\n")

    # Lines after the first line up under the first
    assert_equal(_capture(lambda: log(INFO, "first\nsecond")), "P00   INFO: first\n" + " " * len("P00   INFO: ") + "second\n")

    # A message that is not a string is rendered as one
    assert_equal(_capture(lambda: log(INFO, 404)), "P00   INFO: 404\n")


####################################################################################################################################
def test_log_timestamp():
    """A timestamp is prepended unless it is suppressed."""

    output = _capture(lambda: log(INFO, "message"), timestamp=True)

    assert_true(re.match(r"^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}\.\d{3} P00   INFO: message\n$", output), output)

    # Continuation lines are indented past the timestamp as well
    line_list = _capture(lambda: log(INFO, "first\nsecond"), timestamp=True).split("\n")

    assert_equal(len(line_list[0]) - len("first"), len(line_list[1]) - len("second"))


####################################################################################################################################
def test_log_level():
    """A message above the current level is not written."""

    assert_equal(_capture(lambda: log(DEBUG, "message")), "")
    assert_equal(_capture(lambda: log(DEBUG, "message"), DEBUG), "P00  DEBUG: message\n")

    # Off is below every level so nothing at all is written
    assert_equal(_capture(lambda: log(ERROR, "message"), OFF), "")


####################################################################################################################################
def test_log_write():
    """Every level writes under its own name."""

    for level in (ERROR, WARN, INFO, DETAIL, DEBUG, TRACE):
        assert_equal(
            _capture(lambda: log(level, "message"), TRACE),
            "P00 %6s: message\n" % LEVEL_NAME[level].upper(),
            LEVEL_NAME[level],
        )
