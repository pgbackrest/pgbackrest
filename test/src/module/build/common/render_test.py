"""Test Render Helpers.

The StringId values expected here are taken from StringId macros in the project source, which the linter already checks against the
C encoder, so they are known good rather than whatever this implementation happens to produce."""

####################################################################################################################################
from harness.test import *

from common.render import *


####################################################################################################################################
def test_render_enum():
    """A dashed value becomes camel case."""

    assert_equal(bld_enum(None, "info"), "info")
    assert_equal(bld_enum(None, "type-convert"), "typeConvert")

    # The module name of a test, which is what the generated test.c includes
    assert_equal(bld_enum(None, "common/type-string"), "common/typeString")

    # Successive dashes and a trailing dash have nothing to upper-case
    assert_equal(bld_enum(None, "a--b"), "aB")
    assert_equal(bld_enum(None, "trail-"), "trail")


####################################################################################################################################
def test_render_enum_prefix():
    """A prefix is prepended and upper-cases the first letter of the value."""

    assert_equal(bld_enum("logLevel", "info"), "logLevelInfo")
    assert_equal(bld_enum("logLevel", "off"), "logLevelOff")

    # An empty prefix is not the same as no prefix since it still upper-cases
    assert_equal(bld_enum("", "info"), "Info")

    # An empty value leaves the prefix alone
    assert_equal(bld_enum("prefix", ""), "prefix")


####################################################################################################################################
def test_render_str_id():
    """The rendered macro is what the linter compares the source against."""

    assert_equal(bld_str_id_seq("any"), 'STRID5("any", 0x65c10)')
    assert_equal(bld_str_id_seq("lz4"), 'STRID6("lz4", 0x2068c1)')
    assert_equal(bld_str_id_seq("asc", 1), 'STRID5S("asc", 1, 0xe614)')
    assert_equal(bld_str_id_seq("time", 6), 'STRID5S("time", 6, 0x56a680e)')
    assert_equal(bld_str_id_seq("execId"), 'STRID6("execId", 0x12e0c56051)')
