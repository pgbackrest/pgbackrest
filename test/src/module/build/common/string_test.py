"""Test String Modules.

The StringId values expected here are taken from StringId macros in the project source, which the linter already checks against the
C encoder, so they are known good rather than whatever this implementation happens to produce."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.string_id import *


####################################################################################################################################
def test_string_id_encode_5bit():
    """5-bit encoding covers lower case, dash, and the digits 2, 5, and 6."""

    assert_equal(str_id_from_z("any"), 0x65C10)
    assert_equal(str_id_from_z("bin"), 0x39220)
    assert_equal(str_id_from_z("archive"), 0x16C940E410)

    # Dash and the digits that fit the encoding
    assert_equal(str_id_from_z("blk-incr"), 0x90DC9DAD820)
    assert_equal(str_id_from_z("aes-256-cbc"), 0xC43DFBBCDCCA10)

    # The longest string the encoding holds
    assert_equal(len("backup-error"), STRID5_MAX)
    assert_equal(str_id_from_z("backup-error"), 0x93E522EE1558C220)


####################################################################################################################################
def test_string_id_encode_6bit():
    """6-bit encoding is used when a string does not fit 5-bit, i.e. it has upper case or a digit outside 2, 5, and 6."""

    assert_equal(str_id_from_z("i32"), 0x1E7C91)
    assert_equal(str_id_from_z("lz4"), 0x2068C1)
    assert_equal(str_id_from_z("execId"), 0x12E0C56051)

    assert_equal(str_id_from_z("lz4-dcmp"), 0x40D0C46E068C1)


####################################################################################################################################
def test_string_id_encode_sequence():
    """A sequence below six is held in the header and above that in the high bits."""

    # Held in the header
    assert_equal(str_id_from_z("asc", 1), 0xE614)
    assert_equal(str_id_from_z("auto", 1), 0x7D2A14)
    assert_equal(str_id_from_z("auto", 2), 0x7D2A16)
    assert_equal(str_id_from_z("count", 0), 0x14755E32)
    assert_equal(str_id_from_z("debug", 5), 0x7A88A4C)

    # Held in the high bits
    assert_equal(str_id_from_z("time", 6), 0x56A680E)
    assert_equal(str_id_from_z("lsn", 7), 0x74D81E)
    assert_equal(str_id_from_z("xid", 8), 0x22702E)


####################################################################################################################################
def test_string_id_sequence_read():
    """The sequence can be read back from an encoded value."""

    for value, sequence in (("asc", 1), ("count", 0), ("debug", 5), ("time", 6), ("lsn", 7), ("xid", 8)):
        assert_equal(str_id_seq(str_id_from_z(value, sequence)), sequence, value)


####################################################################################################################################
def test_string_id_encode_error():
    """A string that neither encoding can hold is an error."""

    # Characters outside both encodings
    for value in ("under_score", "dot.dot", "spa ce", "plus+"):
        with assert_raises(ToolError) as error:
            str_id_from_z(value)

        assert_equal(str(error.exception), "'%s' contains invalid characters" % value)

    # Longer than 5-bit holds, and 6-bit holds fewer characters still so it cannot take over
    assert_not_equal(str_id_from_z("a" * STRID5_MAX), 0)

    with assert_raises(ToolError):
        str_id_from_z("a" * (STRID5_MAX + 1))

    # Upper case forces 6-bit, which holds fewer characters
    assert_not_equal(str_id_from_z("A" * STRID6_MAX), 0)

    with assert_raises(ToolError):
        str_id_from_z("A" * (STRID6_MAX + 1))

    # An empty string has nothing to encode
    with assert_raises(ToolError) as error:
        str_id_from_z("")

    assert_equal(str(error.exception), "StringId value may not be empty")
