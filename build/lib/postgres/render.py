"""Render PostgreSQL Interface.

Each version gets a block that includes the vendored header again with every name suffixed by the version, which is how one
translation unit carries an interface for every version it supports. The interfaces are rendered newest first so the most likely
match is found first at run time.

A block only renders the functions that no older version already renders the same code for, since a function whose types and defines
all resolve to the same branches for two versions would compile to the same thing twice. The struct then points both versions at the
one that was rendered, so what is shared is visible where it is used.

The interface being rendered is whichever one the declaration was parsed for, so the binary and the test harness are rendered by the
same code from the same versions."""

####################################################################################################################################
import os

from common.error import ToolError
from common.render import bld_comment_block, bld_define, bld_header
from common.storage import file_read, file_write_differs
from postgres.parse import bld_pg_version_num

_VERSION_MODULE = "postgres-version"
_VERSION_DESCRIPTION = "PostgreSQL Version"

# Base a test system id is offset from, which is the base hrnPgSystemId() adds the version to
_SYSTEM_ID_BASE = 10000000000000000000

# Offsets a test asks for a system id at, since a test that needs a second system id for a version varies it by a small amount
_SYSTEM_ID_OFFSET_LIST = (0, 1)

# Prefix of the defines that are generated into the harness header, which is what identifies them there
_SYSTEM_ID_DEFINE = "HRN_PG_SYSTEMID_"

# Header the system id defines are generated into, which is hand-written apart from them
_PATH_SYSTEM_ID = "test/src/harness/postgres.h"

# Column the comment naming the versions that share a rendering is aligned at, which is the column a #define value is aligned at
_SHARE_COLUMN = 68


####################################################################################################################################
def _version_no_dot(version):
    """The version as it appears in a C name, e.g. "9.6" becomes "96"."""

    return version.replace(".", "")


####################################################################################################################################
def _version_suffix(bld_pg, function, version):
    """The suffix the rendering of a function is named with, given the version that rendered it.

    A rendering only one version uses is named with it, e.g. "14", and a rendering more than one shares is named with the range it
    covers, e.g. "14_19", so which versions share it can be read from the name."""

    index_list = [idx for idx, pg in enumerate(bld_pg.pg_list) if bld_pg.function_version[function][pg.version] == version]

    if len(index_list) == 1:
        return _version_no_dot(version)

    # Versions sharing a rendering are consecutive, since a branch of the vendored header applies from where it begins until the
    # next one begins. A range over versions that are not would name versions that do not share it.
    if index_list[-1] - index_list[0] != len(index_list) - 1:
        raise ToolError(
            "versions %s share %s but are not consecutive"
            % (", ".join(bld_pg.pg_list[idx].version for idx in index_list), function)
        )

    return "%s_%s" % (_version_no_dot(version), _version_no_dot(bld_pg.pg_list[index_list[-1]].version))


####################################################################################################################################
def _function_name(define):
    """The interface function a macro defines, e.g. "PG_INTERFACE_CONTROL_IS" becomes "pgInterfaceControlIs".

    A harness macro reads the same way, e.g. "HRN_PG_INTERFACE_CONTROL" becomes "hrnPgInterfaceControl"."""

    result = ""

    for idx, part in enumerate(define.split("_")):
        part = part.lower()
        result += part[:1].upper() + part[1:] if idx != 0 else part

    return result


####################################################################################################################################
def _render_interface_auto_c(bld_pg):
    """Render the interface, which is one interface per version plus the struct that finds them."""

    interface = bld_pg.interface
    result = bld_header(interface.module, interface.description)

    # Interfaces, newest first
    for pg in reversed(bld_pg.pg_list):
        version_no_dot = _version_no_dot(pg.version)

        # Functions this version renders, which is the ones no older version already renders the same code for
        function_list = [
            function for function in bld_pg.function_list if bld_pg.function_version[function][pg.version] == pg.version
        ]

        # A version that shares every one of its functions has nothing to render, so it gets no block at all
        if function_list == []:
            continue

        result += "\n" + bld_comment_block("PostgreSQL %s interface" % pg.version)
        result += bld_define("PG_VERSION", "PG_VERSION_%s" % version_no_dot) + "\n\n"

        for type in bld_pg.type_list:
            result += bld_define(type, "%s_%s" % (type, version_no_dot)) + "\n"

        # An unreleased version has no catalog version of its own yet, so it accepts any catalog version up to the maximum
        if not pg.release:
            result += "\n#define CATALOG_VERSION_NO_MAX\n"

        result += '\n#include "%s"\n\n' % interface.include

        for function in function_list:
            expand = "%s(%s);" % (function, _version_suffix(bld_pg, function, pg.version))

            # Versions using this rendering rather than one of their own, which says why they have no rendering of it
            share_list = [
                pg_share.version
                for pg_share in bld_pg.pg_list
                if pg_share.version != pg.version and bld_pg.function_version[function][pg_share.version] == pg.version
            ]

            if share_list != []:
                expand += "%*s// Shared with %s" % (_SHARE_COLUMN - len(expand), "", ", ".join(share_list))

            result += expand + "\n"

        # Undefine everything the interface defined so the next one starts clean
        result += "\n" + "".join("#undef %s\n" % type for type in bld_pg.type_list)
        result += "\n" + "".join("#undef %s\n" % define for define in bld_pg.define_list)
        result += "\n" + "".join("#undef %s\n" % function for function in bld_pg.function_list)

    # Interface struct, newest first so the most likely match is found first
    result += "\n" + bld_comment_block(
        "PostgreSQL interface struct\n\nA function shared by more than one version is named for the range it covers, so a version"
        " may name a\nfunction whose name does not begin with it."
    )
    result += "static const %s %s[] =\n{\n" % (interface.type, interface.prefix)

    for pg in reversed(bld_pg.pg_list):
        result += "    {\n        .version = PG_VERSION_%s,\n\n" % _version_no_dot(pg.version)

        for function in bld_pg.function_list:
            name = _function_name(function)
            member = name[len(interface.prefix) :]

            result += "        .%s = %s%s,\n" % (
                member[:1].lower() + member[1:],
                name,
                _version_suffix(bld_pg, function, bld_pg.function_version[function][pg.version]),
            )

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

        result += bld_define("PG_VERSION_%s" % version_no_dot, "%u" % bld_pg_version_num(pg.version)) + "\n"

    # The newest version is the maximum, which the code uses to reject anything newer than it knows about
    result += "\n" + bld_define("PG_VERSION_MAX", "PG_VERSION_%s" % _version_no_dot(bld_pg.pg_list[-1].version)) + "\n"

    result += "\n" + bld_comment_block("PostgreSQL version string constants for use in error messages")

    for pg in bld_pg.pg_list:
        result += bld_define("PG_VERSION_%s_Z" % _version_no_dot(pg.version), '"%s"' % pg.version) + "\n"

    return result + "\n#endif\n"


####################################################################################################################################
def _render_system_id(bld_pg):
    """Render the system id defines, which are the system id the harness writes for a version and the strings a test reads it as.

    A system id is derived from the version so a test that reports one says which version wrote it. The strings are rendered here
    rather than built by the preprocessor because there is no way to make a string of the sum."""

    result = ""

    for pg in bld_pg.pg_list:
        version_no_dot = _version_no_dot(pg.version)

        result += (
            bld_define(_SYSTEM_ID_DEFINE + version_no_dot, "(%uULL + (uint64_t)PG_VERSION_%s)" % (_SYSTEM_ID_BASE, version_no_dot))
            + "\n"
        )

        # An offset is a system id a test asks for by adding to the one for the version, which is how a test uses more than one
        for offset in _SYSTEM_ID_OFFSET_LIST:
            result += (
                bld_define(
                    "%s%s%s_Z" % (_SYSTEM_ID_DEFINE, version_no_dot, "" if offset == 0 else "_%u" % offset),
                    '"%u"' % (_SYSTEM_ID_BASE + bld_pg_version_num(pg.version) + offset),
                )
                + "\n"
            )

    return result


####################################################################################################################################
def bld_pg_render(path_build, bld_pg):
    """Render the PostgreSQL interfaces."""

    file_write_differs(os.path.join(path_build, bld_pg.interface.path_render), _render_interface_auto_c(bld_pg))


####################################################################################################################################
def bld_pg_version_render(path_build, bld_pg):
    """Render the PostgreSQL version constants."""

    file_write_differs(os.path.join(path_build, "src/postgres/version.auto.h"), _render_version_auto_h(bld_pg))


####################################################################################################################################
def bld_pg_system_id_render(path_repo, bld_pg):
    """Render the system id defines into the harness header.

    They go into the header the rest of the harness declarations are in rather than a generated file of their own, so this replaces
    the defines where they are instead of writing a file. Every one of them is replaced by the block, which is rendered where the
    first one was."""

    path = os.path.join(path_repo, _PATH_SYSTEM_ID)
    result = ""
    rendered = False

    for line in file_read(path).rstrip("\n").split("\n"):
        if line.startswith("#define " + _SYSTEM_ID_DEFINE):
            if not rendered:
                result += _render_system_id(bld_pg)
                rendered = True

            continue

        result += line + "\n"

    # A header with none of them is a header this can no longer generate, which is an error rather than a file left as it was
    if not rendered:
        raise ToolError("unable to find %s defines in '%s'" % (_SYSTEM_ID_DEFINE, path))

    file_write_differs(path, result)
