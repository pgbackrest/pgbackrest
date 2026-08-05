"""Render PostgreSQL Interface.

Each version gets a block that includes the vendored header again with every name suffixed by the version, which is how one binary
carries an interface for every version it supports. The interfaces are rendered newest first so the most likely match is found first
at run time."""

####################################################################################################################################
import os

from common.render import bld_comment_block, bld_define, bld_header
from common.storage import file_write_differs

_MODULE = "postgres"
_DESCRIPTION = "PostgreSQL Interface"

_VERSION_MODULE = "postgres-version"
_VERSION_DESCRIPTION = "PostgreSQL Version"


####################################################################################################################################
def _version_no_dot(version):
    """The version as it appears in a C name, e.g. "9.6" becomes "96"."""

    return version.replace(".", "")


####################################################################################################################################
def _version_num(version):
    """The version as the number the C compares, e.g. "9.6" becomes 90600 and "10" becomes 100000."""

    major, _, minor = version.partition(".")

    return int(major) * 10000 + (int(minor) * 100 if minor != "" else 0)


####################################################################################################################################
def _function_name(define):
    """The interface function a macro defines, e.g. "PG_INTERFACE_CONTROL_IS" becomes "pgInterfaceControlIs"."""

    result = ""

    for idx, part in enumerate(define.split("_")):
        part = part.lower()
        result += part[:1].upper() + part[1:] if idx != 0 else part

    return result


####################################################################################################################################
def _render_interface_auto_c(bld_pg):
    """Render interface.auto.c.inc, which is one interface per version plus the struct that finds them."""

    result = bld_header(_MODULE, _DESCRIPTION)

    # Interfaces, newest first
    for pg in reversed(bld_pg.pg_list):
        version_no_dot = _version_no_dot(pg.version)

        result += "\n" + bld_comment_block("PostgreSQL %s interface" % pg.version)
        result += bld_define("PG_VERSION", "PG_VERSION_%s" % version_no_dot) + "\n\n"

        for type in bld_pg.type_list:
            result += bld_define(type, "%s_%s" % (type, version_no_dot)) + "\n"

        # An unreleased version has no catalog version of its own yet, so it accepts any catalog version up to the maximum
        if not pg.release:
            result += "\n#define CATALOG_VERSION_NO_MAX\n"

        result += '\n#include "postgres/interface/version.intern.h"\n\n'

        for function in bld_pg.function_list:
            result += "%s(%s);\n" % (function, version_no_dot)

        # Undefine everything the interface defined so the next one starts clean
        result += "\n" + "".join("#undef %s\n" % type for type in bld_pg.type_list)
        result += "\n" + "".join("#undef %s\n" % define for define in bld_pg.define_list)
        result += "\n" + "".join("#undef %s\n" % function for function in bld_pg.function_list)

    # Interface struct, newest first so the most likely match is found first
    result += "\n" + bld_comment_block("PostgreSQL interface struct")
    result += "static const PgInterface pgInterface[] =\n{\n"

    for pg in reversed(bld_pg.pg_list):
        version_no_dot = _version_no_dot(pg.version)

        result += "    {\n        .version = PG_VERSION_%s,\n\n" % version_no_dot

        for function in bld_pg.function_list:
            name = _function_name(function)
            member = name[len("pgInterface") :]

            result += "        .%s = %s%s,\n" % (member[:1].lower() + member[1:], name, version_no_dot)

        result += "    },\n"

    return result + "};\n"


####################################################################################################################################
def _render_version_auto_h(bld_pg):
    """Render version.auto.h, which is the version numbers and the strings the errors report them with."""

    result = bld_header(_VERSION_MODULE, _VERSION_DESCRIPTION)
    result += "#ifndef POSTGRES_VERSION_AUTO_H\n#define POSTGRES_VERSION_AUTO_H\n"

    result += "\n" + bld_comment_block("PostgreSQL version constants")

    for pg in bld_pg.pg_list:
        version_no_dot = _version_no_dot(pg.version)

        result += bld_define("PG_VERSION_%s" % version_no_dot, "%u" % _version_num(pg.version)) + "\n"

    # The newest version is the maximum, which the code uses to reject anything newer than it knows about
    result += "\n" + bld_define("PG_VERSION_MAX", "PG_VERSION_%s" % _version_no_dot(bld_pg.pg_list[-1].version)) + "\n"

    result += "\n" + bld_comment_block("PostgreSQL version string constants for use in error messages")

    for pg in bld_pg.pg_list:
        result += bld_define("PG_VERSION_%s_Z" % _version_no_dot(pg.version), '"%s"' % pg.version) + "\n"

    return result + "\n#endif\n"


####################################################################################################################################
def bld_pg_render(path_build, bld_pg):
    """Render the PostgreSQL interfaces."""

    file_write_differs(os.path.join(path_build, "src/postgres/interface.auto.c.inc"), _render_interface_auto_c(bld_pg))


####################################################################################################################################
def bld_pg_version_render(path_build, bld_pg):
    """Render the PostgreSQL version constants."""

    file_write_differs(os.path.join(path_build, "src/postgres/version.auto.h"), _render_version_auto_h(bld_pg))
