"""Build Command.

Parses the command line and runs the requested generator. Each generator reads a declaration from the repository and writes the C it
describes into the build path, which is the repository itself for the files that are committed and the build directory for the files
meson generates as it builds.

The documentation generates its own option parse tables and help from its own declarations, so it runs these same generators with the
repository set to the documentation directory."""

####################################################################################################################################
import argparse
import os

from config.parse import bld_cfg_parse
from config.render import bld_cfg_render
from error.parse import bld_err_parse
from error.render import bld_err_render
from help.parse import bld_hlp_parse
from help.render import bld_hlp_render, bld_hlp_render_data

# The help declaration, which lives with the documentation because that is what most of it is for
_PATH_HELP = "doc/xml/reference.xml"
from postgres.parse import bld_pg_parse
from postgres.render import bld_pg_render, bld_pg_version_render


####################################################################################################################################
def _path_help(config):
    """Path of the help declaration."""

    return os.path.join(config.repo_path, _PATH_HELP)


####################################################################################################################################
def _build_config(config):
    """Generate the command and option configuration."""

    bld_cfg_render(config.build_path, bld_cfg_parse(config.repo_path), True)


####################################################################################################################################
def _build_error(config):
    """Generate the error types."""

    bld_err_render(config.build_path, bld_err_parse(config.repo_path))


####################################################################################################################################
def _build_help(config):
    """Generate the help the binary carries for itself."""

    bld_cfg = bld_cfg_parse(config.repo_path)

    bld_hlp_render(config.build_path, bld_cfg, bld_hlp_parse(_path_help(config), bld_cfg, False))


####################################################################################################################################
def _build_help_data(config):
    """Generate the help as raw data, which is what the help unit test loads."""

    bld_cfg = bld_cfg_parse(config.repo_path)

    bld_hlp_render_data(config.build_path, bld_cfg, bld_hlp_parse(_path_help(config), bld_cfg, False))


####################################################################################################################################
def _build_postgres(config):
    """Generate the PostgreSQL interfaces."""

    bld_pg_render(config.build_path, bld_pg_parse(config.repo_path))


####################################################################################################################################
def _build_postgres_version(config):
    """Generate the PostgreSQL version constants."""

    bld_pg_version_render(config.build_path, bld_pg_parse(config.repo_path))


####################################################################################################################################
# Generators, in the order they are listed in the help
_COMMAND_LIST = {
    "config": _build_config,
    "error": _build_error,
    "help": _build_help,
    "help-data": _build_help_data,
    "postgres": _build_postgres,
    "postgres-version": _build_postgres_version,
}


####################################################################################################################################
def cfg_load(arg_list, path_repo):
    """Parse the command line."""

    parser = argparse.ArgumentParser(prog="build.py", description="Generate code for pgBackRest.")
    parser.add_argument("command", choices=_COMMAND_LIST, metavar="COMMAND", help="what to generate: %s" % ", ".join(_COMMAND_LIST))
    parser.add_argument("--repo-path", default=path_repo, metavar="PATH", help="path the declarations are read from")
    parser.add_argument("--build-path", metavar="PATH", help="path the generated code is written to, defaults to the repository")

    result = parser.parse_args(arg_list)

    # A relative path is made absolute so it does not depend on where the tool was run from, which meson does not promise
    result.repo_path = os.path.abspath(result.repo_path)

    # The generated code goes into the repository unless the caller sends it somewhere else, which meson does for the files it
    # generates as it builds rather than the files that are committed
    if result.build_path is None:
        result.build_path = result.repo_path
    else:
        result.build_path = os.path.abspath(result.build_path)

    return result


####################################################################################################################################
def cmd_build(config):
    """Run the requested generator."""

    _COMMAND_LIST[config.command](config)
