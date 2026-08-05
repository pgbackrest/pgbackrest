"""Parse Error Yaml.

Reads the error declarations, which are the definition of record for every error the C can throw. The order they are declared in is
the order they are rendered in, since the generated array is indexed by nothing but position."""

####################################################################################################################################
import os

from common.error import ToolError, check
from common.storage import file_read
from common.yaml import yaml_bool, yaml_load

# Codes an error may use. The range is left open at both ends for the codes the project reserves outside it, e.g. the exit statuses
# the commands themselves return.
CODE_MIN = 25
CODE_MAX = 125


####################################################################################################################################
class BldErrError:
    """An error type."""

    def __init__(self, name, code, fatal):
        self.name = name
        self.code = code
        self.fatal = fatal  # Is the error fatal, i.e. does it mean the code itself is wrong?


####################################################################################################################################
def _code_parse(value, name):
    """Parse and range check an error code."""

    try:
        result = int(value)
    except ValueError:
        raise ToolError("error '%s' code '%s' is not an integer" % (name, value))

    check(CODE_MIN <= result <= CODE_MAX, "error '%s' code must be >= %u and <= %u" % (name, CODE_MIN, CODE_MAX))

    return result


####################################################################################################################################
def bld_err_parse(path_repo):
    """Parse error.yaml into the list of error types."""

    path = os.path.join(path_repo, "build/error.yaml")
    result = []

    for name, detail in yaml_load(file_read(path), path):
        code = None
        fatal = False

        # A scalar is the code on its own, else a map that may also say whether the error is fatal
        if isinstance(detail, str):
            code = detail
        else:
            for key, value in detail:
                if key == "code":
                    code = value
                elif key == "fatal":
                    fatal = yaml_bool(value, "error '%s' fatal" % name)
                else:
                    raise ToolError("unknown error definition '%s'" % key)

        check(code is not None, "error '%s' requires a code" % name)

        result.append(BldErrError(name, _code_parse(code, name), fatal))

    return result
