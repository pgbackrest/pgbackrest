"""Pack Writer.

Writes the compact binary format the C reads a pack with, which is how the help is shipped inside the binary. This is the write side
only and must agree with the reader in src/common/type/pack.c, since what it writes is read back by the C.

A field is a one byte tag, which holds the type and the value, followed by the gap since the last field when it is too wide for the
tag. A field left null is not written at all -- the gap in the ids is what says it was there -- which is what makes the format
compact for data that is mostly defaults."""

####################################################################################################################################
from common.error import check

# Field types, which are the same numbers the reader maps back to types. Only the types the help needs are here, all of which fit in
# the four bits the tag has for a type.
_TYPE_ARRAY = 1
_TYPE_BOOL = 2
_TYPE_OBJ = 5
_TYPE_STR = 7

# Types whose value is one bit in the tag, i.e. the value is there or it is not
_TYPE_SINGLE_BIT = (_TYPE_BOOL, _TYPE_STR)


####################################################################################################################################
class _Tag:
    """Where the ids are up to inside one container, since a container numbers its fields from one."""

    def __init__(self, type):
        self.type = type
        self.id_last = 0
        self.null_total = 0


####################################################################################################################################
class PackWrite:
    """Write a pack."""

    def __init__(self):
        self.buffer = bytearray()
        self._tag_stack = [_Tag(None)]

    ################################################################################################################################
    def _u64(self, value):
        """Write an integer base-128, seven bits at a time, with the high bit set on every byte but the last."""

        while value >= 0x80:
            self.buffer.append((value & 0x7F) | 0x80)
            value >>= 7

        self.buffer.append(value)

    ################################################################################################################################
    def _tag(self, type, id, value):
        """Write a field tag, i.e. the type, the gap since the last field, and as much of the value as fits."""

        tag_top = self._tag_stack[-1]

        # An id that is not given follows the last one, counting the fields that were left null
        if id == 0:
            id = tag_top.id_last + tag_top.null_total + 1
        else:
            check(id > tag_top.id_last, "field id must be greater than last id")

        tag_top.null_total = 0

        # The gap never needs to record the one it is always at least, so it is the difference less one
        tag_id = id - tag_top.id_last - 1
        tag = type << 4

        # A single bit value fits the tag entirely, so nothing of it is left to write after the tag
        if type in _TYPE_SINGLE_BIT:
            tag |= value << 3

            tag |= tag_id & 0x3
            tag_id >>= 2

            if tag_id > 0:
                tag |= 0x4
        else:
            check(value == 0, "no value expected")

            tag |= tag_id & 0x7
            tag_id >>= 3

            if tag_id > 0:
                tag |= 0x8

        self.buffer.append(tag)

        if tag_id > 0:
            self._u64(tag_id)

        tag_top.id_last = id

    ################################################################################################################################
    def null_write(self):
        """Leave a field null, which writes nothing and leaves a gap in the ids."""

        self._tag_stack[-1].null_total += 1

    ################################################################################################################################
    def bool_write(self, value, id=0, default_write=False):
        """Write a boolean, which is left null when it is false unless writing it is forced."""

        if not default_write and value is False:
            self.null_write()

            return

        self._tag(_TYPE_BOOL, id, 1 if value else 0)

    ################################################################################################################################
    def str_write(self, value, id=0):
        """Write a string, which is left null when there is no string at all.

        An empty string is written, since the tag says whether there is a string rather than how long it is."""

        if value is None:
            self.null_write()

            return

        data = value.encode()

        self._tag(_TYPE_STR, id, 1 if len(data) > 0 else 0)

        if len(data) > 0:
            self._u64(len(data))
            self.buffer.extend(data)

    ################################################################################################################################
    def array_begin(self, id=0):
        """Begin an array, which numbers its fields from one."""

        self._tag(_TYPE_ARRAY, id, 0)
        self._tag_stack.append(_Tag(_TYPE_ARRAY))

    ################################################################################################################################
    def array_end(self):
        """End an array."""

        check(self._tag_stack[-1].type == _TYPE_ARRAY, "not in array")

        self._u64(0)
        self._tag_stack.pop()

    ################################################################################################################################
    def obj_begin(self, id=0):
        """Begin an object, which numbers its fields from one."""

        self._tag(_TYPE_OBJ, id, 0)
        self._tag_stack.append(_Tag(_TYPE_OBJ))

    ################################################################################################################################
    def obj_end(self):
        """End an object."""

        check(self._tag_stack[-1].type == _TYPE_OBJ, "not in object")

        self._u64(0)
        self._tag_stack.pop()

    ################################################################################################################################
    def end(self):
        """End the pack and return what was written."""

        check(len(self._tag_stack) == 1, "container not ended")

        self._u64(0)

        return bytes(self.buffer)
