"""Build Documentation.

Builds everything the renderers work from: the command and configuration references, the user guide with everything in it resolved, and
the manual page. All four come from the declarations the binary is generated from, so the documentation describes what the code does.

The variables given on the command line are loaded before the user guide is read, so a document cannot override what the caller asked
for."""

####################################################################################################################################
import argparse
import os

from command.build.man import reference_man_render
from command.build.pre import build_pre
from command.build.reference import reference_command_render, reference_configuration_render
from command.build.var_store import VarStore
from common.error import ToolError
from common.log import LEVEL_NAME
from common.storage import file_read, file_write
from common.xml import xml_document_parse, xml_parse
from config.parse import bld_cfg_parse
from help.parse import bld_hlp_parse

# Where the documentation is read from and written to, relative to the repository
_PATH_XML = "doc/xml"
_PATH_OUT_XML = "doc/output/xml"
_PATH_OUT_MAN = "doc/output/man"

_FILE_MAN = "pgbackrest.1.txt"


####################################################################################################################################
def cfg_load(arg_list, path_repo):
    """Parse the command line."""

    parser = argparse.ArgumentParser(prog="doc.py", description="Build the pgBackRest documentation.")
    parser.add_argument("--repo-path", default=path_repo, metavar="PATH", help="code repository path")
    parser.add_argument("--var", action="append", default=[], metavar="KEY=VALUE", help="variable to set in the documentation")
    parser.add_argument(
        "--log-level", default="info", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="console log level"
    )
    parser.add_argument("--no-log-timestamp", dest="log_timestamp", action="store_false", help="suppress timestamps in the log")

    result = parser.parse_args(arg_list)

    # A relative path is made absolute so it does not depend on where the tool was run from
    result.repo_path = os.path.abspath(result.repo_path)

    # Variables are given as key=value, and a value may itself hold an equals sign
    result.var_map = {}

    for var in result.var:
        key, _, value = var.partition("=")

        if key == "" or "=" not in var:
            raise ToolError("variable '%s' must be given as key=value" % var)

        result.var_map[key] = value

    return result


####################################################################################################################################
def _read(path_repo, path, document=False):
    """Read xml from the repository."""

    path = os.path.join(path_repo, path)
    content = file_read(path)

    return xml_document_parse(content, path) if document else xml_parse(content, path)


####################################################################################################################################
def cmd_build(path_repo, var_map):
    """Build the documentation."""

    var_store = VarStore()

    # The caller's variables are loaded first, so a document declaring the same one does not override them
    for variable, value in var_map.items():
        var_store.add(variable, value)

    bld_cfg = bld_cfg_parse(path_repo)
    bld_hlp = bld_hlp_parse(os.path.join(path_repo, _PATH_XML, "reference.xml"), bld_cfg, True)

    index = _read(path_repo, os.path.join(_PATH_XML, "index.xml"))
    user_guide = _read(path_repo, os.path.join(_PATH_XML, "user-guide.xml"), document=True)

    file_write(os.path.join(path_repo, _PATH_OUT_XML, "command.xml"), reference_command_render(bld_cfg, bld_hlp).render())
    file_write(
        os.path.join(path_repo, _PATH_OUT_XML, "configuration.xml"), reference_configuration_render(bld_cfg, bld_hlp).render()
    )
    file_write(os.path.join(path_repo, _PATH_OUT_XML, "user-guide.xml"), build_pre(user_guide, bld_hlp, var_store).render())
    file_write(os.path.join(path_repo, _PATH_OUT_MAN, _FILE_MAN), reference_man_render(index, bld_cfg, bld_hlp))
