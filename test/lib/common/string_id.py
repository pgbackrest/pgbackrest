"""Represent short strings as integers.

Port of the encoding in src/common/type/stringId.c. The maps there are written out as 256 byte tables; here they are built from the
character ranges they encode, which is the same mapping."""

####################################################################################################################################
from common.error import TestError

# Maximum characters that can be encoded with each number of bits
STRID5_MAX = 12
STRID6_MAX = 10

# Sequence value meaning no sequence was specified
STRING_ID_SEQ_NONE = None

# Sequence at or above this must be stored in the high bits rather than the header
_SEQ_HIGH_MIN = 6

# Bits used to encode a character, and the mask for the low sequence bits
_BIT5 = 0
_BIT6 = 1
_SEQ_LOW_MASK = 0xE


####################################################################################################################################
def _map_build(bit):
    """Build a character to encoding map for the specified number of bits. A character not in the map cannot be encoded."""

    result = {}

    for index, char in enumerate("abcdefghijklmnopqrstuvwxyz"):
        result[char] = index + 1

    result["-"] = 27

    # 5-bit encoding has room for only a few digits while 6-bit encoding covers all digits and upper case
    if bit == _BIT5:
        result["2"] = 28
        result["5"] = 29
        result["6"] = 30
    else:
        for index, char in enumerate("0123456789"):
            result[char] = index + 28

        for index, char in enumerate("ABCDEFGHIJKLMNOPQRSTUVWXYZ"):
            result[char] = index + 38

    return result


_MAP = {_BIT5: _map_build(_BIT5), _BIT6: _map_build(_BIT6)}


####################################################################################################################################
def _str_id_bit_from_z(bit, value):
    """Encode with the specified number of bits, or return 0 when the value does not fit the encoding."""

    if len(value) > (STRID5_MAX if bit == _BIT5 else STRID6_MAX):
        return 0

    map = _MAP[bit]

    for char in value:
        if char not in map:
            return 0

    # Set encoding in the header then shift each character into place
    result = bit
    shift = 5 if bit == _BIT5 else 6

    for index, char in enumerate(value):
        result |= map[char] << (4 + shift * index)

    return result


####################################################################################################################################
def str_id_from_z(value, sequence=STRING_ID_SEQ_NONE):
    """Encode a string as a StringId, adding a sequence when one is specified."""

    if value == "":
        raise TestError("StringId value may not be empty")

    result = _str_id_bit_from_z(_BIT5, value)

    # If 5-bit encoding fails try 6-bit
    if result == 0:
        result = _str_id_bit_from_z(_BIT6, value)

        # Error when 6-bit encoding also fails
        if result == 0:
            raise TestError("'%s' contains invalid characters" % value)

    if sequence is not STRING_ID_SEQ_NONE:
        # If the sequence fits in the header
        if sequence < _SEQ_HIGH_MIN:
            result |= (sequence + 1) << 1
        # Else the sequence must be stored in the high bits
        else:
            bit = result & 1

            # Shift to remove the bit marker (added back below) and make space for the high sequence
            result >>= 1
            result <<= 6

            result |= (sequence - _SEQ_HIGH_MIN) << 4 | _SEQ_LOW_MASK | bit

    return result


####################################################################################################################################
def str_id_seq(str_id):
    """Extract the sequence from a StringId."""

    sequence = str_id & _SEQ_LOW_MASK

    if sequence != _SEQ_LOW_MASK:
        return (sequence >> 1) - 1

    return ((str_id & 0x1F0) >> 4) + _SEQ_HIGH_MIN
