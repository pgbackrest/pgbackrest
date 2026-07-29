"""Render Helpers.

Port of the naming helpers in src/build/common/render.c."""

####################################################################################################################################
from common.string_id import STRING_ID_SEQ_NONE, str_id_from_z, str_id_seq


####################################################################################################################################
def bld_enum(prefix, value):
    """Convert a dashed value to an enum name, e.g. "type-convert" becomes "typeConvert".

    When a prefix is given it is prepended and the first letter of the value is upper-cased, e.g. bld_enum("logLevel", "info")
    becomes "logLevelInfo"."""

    result = ""
    upper = False

    if prefix is not None:
        result = prefix
        upper = True

    for char in value:
        # A dash is removed and the letter that follows it is upper-cased
        if char == "-":
            upper = True
            continue

        result += char.upper() if upper else char
        upper = False

    return result


####################################################################################################################################
def bld_str_id_seq(value, sequence=STRING_ID_SEQ_NONE):
    """Render the StringId macro for a value, e.g. STRID5("test", 0x2a7250), which is the form the source is expected to contain."""

    str_id = str_id_from_z(value, sequence)

    if sequence is STRING_ID_SEQ_NONE:
        return 'STRID%u("%s", 0x%x)' % ((str_id & 1) + 5, value, str_id)

    return 'STRID%uS("%s", %u, 0x%x)' % ((str_id & 1) + 5, value, str_id_seq(str_id), str_id)
