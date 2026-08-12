"""Render PostgreSQL Interface.

Each version gets a block that includes the vendored header again with every name suffixed by the version, which is how one
translation unit carries an interface for every version it supports. The interfaces are rendered newest first so the most likely
match is found first at run time.

A block only renders the functions that no other version already renders the same code for, since a function whose types and
defines all resolve to the same branches for two versions would compile to the same thing twice. The struct then points both
versions at the one that was rendered, so what is shared is visible where it is used.

The values a version carries are macros of the vendored header, which have a value only inside its block, so the block captures
them as constants named for the version and the struct names those.

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

# Member marking a version that has not been released, which the declaration says rather than the interface header
_UNRELEASED_MEMBER = "unreleased"

# Macro capturing the values of a version as constants named for it, which is rendered in the block since that is the only place
# they have a value
_VALUE_MACRO = "%s_VALUE"

# Macro matching a version against an interface, which is rendered once after the versions when the interface declares it. The
# range form is rendered when a version has not been released, since only then does an interface accept more than one value.
_MATCH_MACRO = "%s_MATCH"
_MATCH_MACRO_RANGE = "%s_MATCH_RANGE"


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
    """The interface function a macro defines, e.g. "PG_INTERFACE_CONTROL" becomes "pgInterfaceControl".

    A harness macro reads the same way, e.g. "HRN_PG_INTERFACE_CONTROL" becomes "hrnPgInterfaceControl"."""

    result = ""

    for idx, part in enumerate(define.split("_")):
        part = part.lower()
        result += part[:1].upper() + part[1:] if idx != 0 else part

    return result


####################################################################################################################################
def _render_macro_list(bld_pg, macro_list, pg):
    """Macros of a list that a version renders, which are the ones no other version already renders the same code for."""

    return [macro for macro in macro_list if bld_pg.function_version[macro][pg.version] == pg.version]


####################################################################################################################################
def _render_expand(bld_pg, macro_list, pg):
    """Render the macros a version expands, each saying which other versions share the rendering."""

    result = ""

    for macro in macro_list:
        expand = "%s(%s);" % (macro, _version_suffix(bld_pg, macro, pg.version))

        # Versions using this rendering rather than one of their own, which says why they have no rendering of it
        share_list = [
            pg_share.version
            for pg_share in bld_pg.pg_list
            if pg_share.version != pg.version and bld_pg.function_version[macro][pg_share.version] == pg.version
        ]

        if share_list != []:
            expand += "%*s// Shared with %s" % (_SHARE_COLUMN - len(expand), "", ", ".join(share_list))

        result += expand + "\n"

    return result


####################################################################################################################################
def _render_interface_auto_c(bld_pg):
    """Render the interface, which is one interface per version plus the struct that finds them."""

    interface = bld_pg.interface
    result = bld_header(interface.module, interface.description)

    # Interfaces, newest first so the most likely match is found first at run time
    for pg in reversed(bld_pg.pg_list):
        version_no_dot = _version_no_dot(pg.version)

        # Values and functions this version renders, which are the ones no other version already renders the same code for
        value_list = _render_macro_list(bld_pg, list(dict.fromkeys(value.macro for value in bld_pg.value_list)), pg)
        function_list = _render_macro_list(bld_pg, bld_pg.function_list, pg)

        result += "\n" + bld_comment_block("PostgreSQL %s interface" % pg.version)

        # Only the interface declaring the vendored types sets the version they are declared for. The other renames them to
        # what that one declared, which is why they are named the same way here.
        if interface.vendor:
            result += bld_define("PG_VERSION", "PG_VERSION_%s" % version_no_dot) + "\n\n"

        for type in bld_pg.type_list:
            result += bld_define(type, "%s_%s" % (type, version_no_dot)) + "\n"

        # An unreleased version has no catalog version of its own yet, so it accepts any catalog version up to the maximum
        if interface.vendor and not pg.release:
            result += "\n#define CATALOG_VERSION_NO_MAX\n"

        result += '\n#include "%s"\n\n' % interface.include

        # Values this version captures as constants named for it, so the struct at the end can name them
        result += _render_expand(bld_pg, value_list, pg)

        # A value is read where a function is called, so what a version captures is separated from what it renders
        if value_list != [] and function_list != []:
            result += "\n"

        result += _render_expand(bld_pg, function_list, pg)

        # Undefine everything the interface defined so the next one starts clean
        result += "\n" + "".join("#undef %s\n" % type for type in bld_pg.type_list)

        # Only the interface declaring the vendored types defines what they define, so only it has them to undefine
        if interface.vendor:
            result += "\n" + "".join("#undef %s\n" % define for define in bld_pg.define_list)

        result += "\n" + "".join("#undef %s\n" % function for function in bld_pg.function_list)

    # Matches, rendered once each in the form that fits the versions that were rendered. Only a version that has not been released
    # accepts a range of values, so the range form is rendered only while there is one.
    release = all(pg.release for pg in bld_pg.pg_list)

    for macro in [macro for macro in bld_pg.macro_list if macro.endswith(_MATCH_MACRO % "")]:
        prefix = macro[: -len(_MATCH_MACRO % "")]
        member = _function_name(prefix)[len(interface.prefix) :]

        result += "\n" + bld_comment_block("PostgreSQL %s match" % (member[:1].lower() + member[1:]))
        result += "%s();\n" % ((_MATCH_MACRO if release else _MATCH_MACRO_RANGE) % prefix)

    # Interface struct, newest first so the most likely match is found first
    result += "\n" + bld_comment_block(
        "PostgreSQL interface struct\n\nA function shared by more than one version is named for the range it covers, so a version"
        " may name a\nfunction whose name does not begin with it."
    )
    result += "static const %s %s[] =\n{\n" % (interface.type, interface.prefix)

    for pg in reversed(bld_pg.pg_list):
        version_no_dot = _version_no_dot(pg.version)

        result += "    {\n        .version = PG_VERSION_%s,\n" % version_no_dot

        # A version that has not been released accepts a range of catalog versions rather than only its own
        if interface.unreleased and not pg.release:
            result += "        .%s = true,\n" % _UNRELEASED_MEMBER

        result += "\n"

        # An interface reading the values from another one carries none of its own, so there is no group to separate
        if bld_pg.value_list != []:
            for value in bld_pg.value_list:
                member = value.name[len(interface.prefix) :]

                result += "        .%s = %s%s,\n" % (
                    member[:1].lower() + member[1:],
                    value.name,
                    _version_suffix(bld_pg, value.macro, bld_pg.function_version[value.macro][pg.version]),
                )

            result += "\n"

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
