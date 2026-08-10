"""Render Error Data.

Writes the error type declarations and definitions. The definitions include the list the C indexes by code, which is why an error
can be removed from the declaration but its code should not be reused."""

####################################################################################################################################
import os

from common.render import bld_comment_block, bld_enum, bld_header
from common.storage import file_write_differs

_MODULE = "error"
_DESCRIPTION = "Error Type Definition"


####################################################################################################################################
def _err_name(name):
    """Build the C name of an error, e.g. "file-open" becomes "FileOpenError"."""

    return bld_enum("", name) + "Error"


####################################################################################################################################
def _render_error_auto_h(err_list):
    """Render error.auto.h, which declares every error type."""

    result = bld_header(_MODULE, _DESCRIPTION) + "#ifndef COMMON_ERROR_ERROR_AUTO_H\n#define COMMON_ERROR_ERROR_AUTO_H\n"

    result += "\n" + bld_comment_block("Error type declarations")

    for err in err_list:
        result += "ERROR_DECLARE(%s);\n" % _err_name(err.name)

    return result + "\n#endif\n"


####################################################################################################################################
def _render_error_auto_c(err_list):
    """Render error.auto.c.inc, which defines every error type and the array the C looks them up in."""

    result = bld_header(_MODULE, _DESCRIPTION)

    result += "\n" + bld_comment_block("Error type definitions")

    for err in err_list:
        result += "ERROR_DEFINE(%3u, %s, %s, RuntimeError);\n" % (err.code, _err_name(err.name), "true" if err.fatal else "false")

    result += "\n" + bld_comment_block("Error type array")
    result += "static const ErrorType *const errorTypeList[] =\n{\n"

    for err in err_list:
        result += "    &%s,\n" % _err_name(err.name)

    return result + "    NULL,\n};\n"


####################################################################################################################################
def bld_err_render(path_build, err_list):
    """Render the error files."""

    file_write_differs(os.path.join(path_build, "src/common/error/error.auto.h"), _render_error_auto_h(err_list))
    file_write_differs(os.path.join(path_build, "src/common/error/error.auto.c.inc"), _render_error_auto_c(err_list))
