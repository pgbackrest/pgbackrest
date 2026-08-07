"""Test Pack Writer.

The bytes expected here are read the way the format documents them, i.e. a tag byte holding the type, the value, and the gap since
the last field, so a failure says which part of the tag is wrong rather than only that a byte differs."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.pack import *


####################################################################################################################################
def test_pack_bool():
    """A boolean is one bit of the tag, and false is left null since that is the default."""

    pack = PackWrite()
    pack.bool_write(True)

    # 2 = bool type, 8 = value bit
    assert_equal(pack.end(), b"\x28\x00")

    # False writes nothing at all, so the pack holds only its terminator
    pack = PackWrite()
    pack.bool_write(False)

    assert_equal(pack.end(), b"\x00")

    # Unless writing it is forced, which is what says the value is there and is false
    pack = PackWrite()
    pack.bool_write(False, default_write=True)

    assert_equal(pack.end(), b"\x20\x00")


####################################################################################################################################
def test_pack_str():
    """A string is a tag saying whether there is data, then the size and the bytes."""

    pack = PackWrite()
    pack.str_write("sample")

    # 7 = string type, 8 = data bit, 06 = size, then the bytes
    assert_equal(pack.end(), b"\x78\x06sample\x00")

    # An empty string is written, since the tag says whether there is a string rather than how long it is
    pack = PackWrite()
    pack.str_write("")

    assert_equal(pack.end(), b"\x70\x00")

    # No string at all is left null
    pack = PackWrite()
    pack.str_write(None)

    assert_equal(pack.end(), b"\x00")

    # A string longer than a single byte size, which is where the size takes two bytes
    pack = PackWrite()
    pack.str_write("x" * 200)

    assert_equal(pack.end(), b"\x78\xc8\x01" + b"x" * 200 + b"\x00")


####################################################################################################################################
def test_pack_id():
    """A field that is left null leaves a gap in the ids, which is what says it was there."""

    # A gap of one is never recorded, since there is always at least that much, so one null leaves a recorded gap of one
    pack = PackWrite()
    pack.null_write()
    pack.bool_write(True)

    # 2 = bool, 8 = value bit, 1 = the low bits of the gap
    assert_equal(pack.end(), b"\x29\x00")

    # A gap too wide for the two bits the tag has for it, which is what the more-gap bit says
    pack = PackWrite()

    for _ in range(40):
        pack.null_write()

    pack.bool_write(True)

    # 4 = more gap follows, then the gap shifted right by the two bits already in the tag
    assert_equal(pack.end(), b"\x2c\x0a\x00")

    # An id given explicitly leaves the same gap as the nulls it skips over would
    pack = PackWrite()
    pack.bool_write(True, id=4)

    assert_equal(pack.end(), b"\x2b\x00")

    pack = PackWrite()
    pack.bool_write(True, id=2)

    with assert_raises(ToolError) as error:
        pack.bool_write(True, id=1)

    assert_equal(str(error.exception), "field id must be greater than last id")


####################################################################################################################################
def test_pack_container():
    """A container numbers its fields from one and is ended with a zero."""

    pack = PackWrite()
    pack.array_begin()
    pack.bool_write(True)
    pack.bool_write(True)
    pack.array_end()

    # 1 = array type, then two bools, then the array end and the pack end
    assert_equal(pack.end(), b"\x10\x28\x28\x00\x00")

    # An object at an id of its own, which is how the reader knows which command an override is for
    pack = PackWrite()
    pack.obj_begin(id=2)
    pack.str_write("x")
    pack.obj_end()

    assert_equal(pack.end(), b"\x51\x78\x01x\x00\x00")

    # A container has three bits of the tag for the gap rather than two, since it has no value to hold
    pack = PackWrite()
    pack.array_begin(id=9)
    pack.array_end()

    assert_equal(pack.end(), b"\x18\x01\x00\x00")

    # A container must be ended before the pack is, and with the end that matches it
    pack = PackWrite()
    pack.array_begin()

    with assert_raises(ToolError) as error:
        pack.obj_end()

    assert_equal(str(error.exception), "not in object")

    with assert_raises(ToolError) as error:
        pack.end()

    assert_equal(str(error.exception), "container not ended")

    pack = PackWrite()
    pack.obj_begin()

    with assert_raises(ToolError) as error:
        pack.array_end()

    assert_equal(str(error.exception), "not in array")
