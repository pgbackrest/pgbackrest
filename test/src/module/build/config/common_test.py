"""Test Config Value Parsing.

These must agree with the C, which parses the same values at run time, so the expectations here are the byte and millisecond counts the
C is known to produce rather than whatever this implementation happens to produce."""

####################################################################################################################################
from harness.test import *

from common.error import *
from config.common import *


####################################################################################################################################
def test_cfg_parse_size():
    """A size is the number and the qualifier it is multiplied by."""

    # No qualifier is bytes
    assert_equal(cfg_parse_size("0"), 0)
    assert_equal(cfg_parse_size("1024"), 1024)

    # Every qualifier, in each of the spellings that are allowed
    assert_equal(cfg_parse_size("5b"), 5)
    assert_equal(cfg_parse_size("1k"), 1024)
    assert_equal(cfg_parse_size("1kb"), 1024)
    assert_equal(cfg_parse_size("1kib"), 1024)
    assert_equal(cfg_parse_size("1KiB"), 1024)
    assert_equal(cfg_parse_size("1m"), 1048576)
    assert_equal(cfg_parse_size("1g"), 1073741824)
    assert_equal(cfg_parse_size("1t"), 1099511627776)
    assert_equal(cfg_parse_size("1p"), 1125899906842624)

    for value in ("", "1x", "kb", "1.5g", "-1"):
        with assert_raises(ToolError) as error:
            cfg_parse_size(value)

        assert_equal(str(error.exception), "value '%s' is not valid" % value)


####################################################################################################################################
def test_cfg_parse_time():
    """A time is the number in milliseconds, which is the unit the C keeps it in."""

    # No qualifier is seconds
    assert_equal(cfg_parse_time("0"), 0)
    assert_equal(cfg_parse_time("30"), 30000)

    # Every qualifier, including the only one that is two characters
    assert_equal(cfg_parse_time("100ms"), 100)
    assert_equal(cfg_parse_time("100MS"), 100)
    assert_equal(cfg_parse_time("30s"), 30000)
    assert_equal(cfg_parse_time("15m"), 15 * 60 * 1000)
    assert_equal(cfg_parse_time("1h"), 60 * 60 * 1000)
    assert_equal(cfg_parse_time("1d"), 24 * 60 * 60 * 1000)
    assert_equal(cfg_parse_time("1w"), 7 * 24 * 60 * 60 * 1000)

    # A qualifier that is not one, no number to go with the qualifier, and a number that is not one
    for value in ("30x", "s", "", "1.5s"):
        with assert_raises(ToolError) as error:
            cfg_parse_time(value)

        assert_equal(str(error.exception), "value '%s' is not valid" % value)
