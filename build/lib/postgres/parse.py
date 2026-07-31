"""Parse PostgreSQL Interface Declaration.

The versions come from postgres.yaml, but the types, defines, and functions that make up an interface are read out of the vendored
PostgreSQL headers rather than declared. A version interface is the same headers compiled again with different names, so scanning
them is what keeps the generated code in step with a header update."""

####################################################################################################################################
import os

from common.error import ToolError
from common.storage import file_read
from common.yaml import yaml_bool, yaml_load

# Defines that every interface undefines but that no vendored header declares, so they are added to the list explicitly
_DEFINE_EXTRA_LIST = ("CATALOG_VERSION_NO_MAX", "PG_VERSION")


####################################################################################################################################
class BldPgVersion:
    """A supported PostgreSQL version."""

    def __init__(self, version, release):
        self.version = version
        self.release = release  # Has the version been released?


####################################################################################################################################
class BldPg:
    """The PostgreSQL interface declaration."""

    def __init__(self, pg_list, type_list, define_list, function_list):
        self.pg_list = pg_list  # Supported versions, oldest first
        self.type_list = type_list  # Interface types, sorted
        self.define_list = define_list  # Interface defines, sorted
        self.function_list = function_list  # Functions defined by macros, in the order they are declared


####################################################################################################################################
def bld_pg_version_list(path_repo):
    """Parse the supported versions, oldest first.

    Separate from the interface below because the versions are the whole of what the declaration says, so a tool that needs to know
    which versions are supported does not also scan the vendored headers for an interface it has no use for."""

    path = os.path.join(path_repo, "build/postgres.yaml")
    result = []

    for key, value in yaml_load(file_read(path), path):
        if key != "version":
            raise ToolError("unknown postgres definition '%s'" % key)

        for version in value:
            # A scalar is the version on its own, else a map naming the version and its attributes
            if isinstance(version, str):
                result.append(BldPgVersion(version, True))

                continue

            for name, detail in version:
                release = True

                for def_key, def_value in detail:
                    if def_key != "release":
                        raise ToolError("unknown postgres definition '%s'" % def_key)

                    release = yaml_bool(def_value, "version '%s' release" % name)

                result.append(BldPgVersion(name, release))

    return result


####################################################################################################################################
def _define_list(header):
    """Scan the defines out of a header."""

    result = []

    for line in header.split("\n"):
        line = line.strip()

        if not line.startswith("#define"):
            continue

        token = line.split(" ")[1].strip()

        if token == "":
            raise ToolError("unable to find define -- are there extra spaces on '%s'" % line)

        # The define name may be followed by a parameter list or separated from its value by a tab
        define = token.split("(")[0] if "(" in token else token.split("\t")[0]
        define = define.strip()

        if define not in result:
            result.append(define)

    return result


####################################################################################################################################
def _type_list(header):
    """Scan the types out of a header.

    A typedef of a struct or an enum names the type after the block rather than before it, so the scan carries on to the closing
    brace, collecting the values of an enum on the way since each is a name the interface has to undefine."""

    result = []
    scan_enum = False

    def add(value):
        if value not in result:
            result.append(value)

    for line in header.split("\n"):
        token_list = line.strip().split(" ")

        if token_list[0] == "typedef":
            # A struct or an enum is named at the end of the block, so keep scanning
            if token_list[1] in ("struct", "enum"):
                scan_enum = token_list[1] == "enum"
            else:
                add(token_list[-1].split(";")[0])
        elif token_list[0] == "}":
            add(token_list[-1].split(";")[0])
            scan_enum = False
        elif scan_enum and token_list[0] != "{":
            add(token_list[0].split(",")[0])

    return result


####################################################################################################################################
def bld_pg_parse(path_repo):
    """Parse the PostgreSQL interface declaration."""

    path_vendor = os.path.join(path_repo, "src/postgres/interface/version.vendor.h")
    header_vendor = file_read(path_vendor)

    # The interface is generated from the vendored header, so its types and defines are whatever that header declares
    type_list = sorted(_type_list(header_vendor))
    define_list = sorted(_define_list(header_vendor) + list(_DEFINE_EXTRA_LIST))

    # Functions are defined as macros, which each interface expands for its own version
    function_list = _define_list(file_read(os.path.join(path_repo, "src/postgres/interface/version.intern.h")))

    return BldPg(bld_pg_version_list(path_repo), type_list, define_list, function_list)
