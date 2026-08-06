"""Config Value Parsing.

Size and time options are written the way a person would write them, e.g. 1GiB or 30s, so the generated rules need the value they
mean in order to sort them and to compare a value against a range. This is a port of the parsing the C does at run time and must agree
with it, since a value the C parses differently would be checked against the wrong range.

Only the parsing the generated code needs is here. The C also parses these at run time from user input, where the errors matter."""

####################################################################################################################################
import re

from common.error import ToolError

# Size qualifiers, i.e. what the letter on the end of a size multiplies the number by
_SIZE_QUALIFIER = {
    "b": 1,
    "k": 1024,
    "m": 1024 * 1024,
    "g": 1024 * 1024 * 1024,
    "t": 1024 * 1024 * 1024 * 1024,
    "p": 1024 * 1024 * 1024 * 1024 * 1024,
}

_SIZE_EXP = re.compile(r"^[0-9]+(kib|kb|k|mib|mb|m|gib|gb|g|tib|tb|t|pib|pb|p|b)*$")

# Time qualifiers, in milliseconds, which is the unit the C keeps a time option in
_TIME_QUALIFIER = {
    "ms": 1,
    "s": 1000,
    "m": 60 * 1000,
    "h": 60 * 60 * 1000,
    "d": 24 * 60 * 60 * 1000,
    "w": 7 * 24 * 60 * 60 * 1000,
}

# A time with no qualifier is in seconds
_TIME_MULTIPLIER_DEFAULT = 1000


####################################################################################################################################
def cfg_parse_size(value):
    """The number of bytes a size value means, e.g. "1GiB" is 1073741824."""

    value_lower = value.lower()

    if _SIZE_EXP.match(value_lower) is None:
        raise ToolError("value '%s' is not valid" % value)

    # Find the qualifier, which is the first letter of whatever follows the number. A trailing b may be the qualifier itself or the
    # bytes of a longer qualifier, e.g. the b in kb or in kib.
    number = value_lower.rstrip("abcdefghijklmnopqrstuvwxyz")
    qualifier = value_lower[len(number) : len(number) + 1]

    return int(number) * (_SIZE_QUALIFIER[qualifier] if qualifier != "" else 1)


####################################################################################################################################
def cfg_parse_time(value):
    """The number of milliseconds a time value means, e.g. "30s" is 30000 and "100ms" is 100."""

    # The qualifier is what follows the number, which is ms or a single letter or nothing at all
    number = value.rstrip("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")
    qualifier = value[len(number) :].lower()

    if qualifier != "" and qualifier not in _TIME_QUALIFIER:
        raise ToolError("value '%s' is not valid" % value)

    if number == "":
        raise ToolError("value '%s' is not valid" % value)

    try:
        return int(number) * (_TIME_QUALIFIER[qualifier] if qualifier != "" else _TIME_MULTIPLIER_DEFAULT)
    except ValueError:
        raise ToolError("value '%s' is not valid" % value)
