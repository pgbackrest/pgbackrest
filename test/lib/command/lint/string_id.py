"""StringId Linter.

Checks that every StringId macro in the source encodes the string it claims to. The encoded value is written into the source so it
can be used in a switch, which means a hand-edited string and value can drift apart without the compiler noticing."""

####################################################################################################################################
import re

from common.error import TestError
from common.log import *
from common.render import bld_str_id_seq

# Macro invocations to check, e.g. STRID5("test", 0x2a7250) or STRID6S("test", 1, 0x1e2a7250)
_STRID_EXP = re.compile(r"STRID(5|6)(S?)\([^)]+\)")

# The macro definitions themselves, which name their parameters rather than encoding a string
_STRID_DEFINE_LIST = (
    "STRID5(str, strId)",
    "STRID5S(str, seq, strId)",
    "STRID6(str, strId)",
    "STRID6S(str, seq, strId)",
)


####################################################################################################################################
def lint_str_id(source):
    """Check the StringId macros in a source file and return the number of errors found."""

    result = 0

    for match in _STRID_EXP.finditer(source):
        text = match.group(0)
        param_list = [param.strip() for param in text[text.find("(") + 1 : -1].split(",")]
        param = param_list[0]

        # Skip macro definitions
        if text in _STRID_DEFINE_LIST:
            continue

        # Skip test strings, i.e. a quoted string inside a string
        if param.startswith('\\"') and param.endswith('\\"'):
            continue

        # Skip test values
        if len(param_list) > 1 and param_list[1].startswith("TEST_"):
            continue

        # Param must begin and end with a quote
        if not param.startswith('"') or not param.endswith('"'):
            log(WARN, "'%s' must have quotes around string parameter '%s'" % (text, param))
            result += 1

            continue

        # Check validity of the string
        try:
            if match.group(2) == "":
                expected = bld_str_id_seq(param[1:-1])
            else:
                expected = bld_str_id_seq(param[1:-1], int(param_list[1]))
        except TestError as error:
            log(WARN, "'%s' is not valid: %s" % (text, error))
            result += 1

            continue

        if text != expected:
            log(WARN, "'%s' should be '%s'" % (text, expected))
            result += 1

    return result
