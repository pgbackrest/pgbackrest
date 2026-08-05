"""Build the Documentation.

Builds everything the documentation is made of and renders it. The documents that are generated from the declarations the binary is
built from -- the command and configuration references, the user guide with everything in it resolved, and the release notes -- are
built here and handed to the renderers in memory, so what is rendered is what was just built rather than a file that may be stale.

The user guide runs real commands on real hosts as it is built, so a run can take a long time. What those commands wrote is cached,
which is what makes it possible to render the documentation again without running any of it."""

####################################################################################################################################
import argparse
import os
import shutil
import time

from common.date import date_render
from common.error import ToolError
from common.log import *
from common.storage import file_read, file_write, path_create, path_list
from common.var_store import VarStore
from common.xml import xml_document_parse, xml_node_attribute, xml_node_child_list, xml_node_content, xml_node_normalize
from command.build.man import reference_man_render
from command.build.news import news_index_render, news_render
from command.build.pre import build_pre
from command.build.reference import reference_command_render, reference_configuration_render
from command.render.host import image_build_cached
from command.render.html import html_render
from command.render.manifest import RENDER_HTML, RENDER_MAN, RENDER_MARKDOWN, Manifest
from command.render.markdown import markdown_render
from command.render.release import release_current_stable, release_last, release_render
from config.parse import bld_cfg_parse
from config.project import PROJECT_EXE, project_version
from help.parse import bld_hlp_parse

# Where the documentation is read from and written to, relative to the documentation path
_PATH_XML = "xml"
_PATH_OUT = "output"

_FILE_MAN = "pgbackrest.1.txt"


####################################################################################################################################
def cfg_load(arg_list, path_repo):
    """Parse the command line and apply the rules that cannot be expressed in the parser."""

    parser = argparse.ArgumentParser(prog="doc.py", description="pgBackRest Documentation Builder")

    parser.add_argument("--version", action="version", version="pgBackRest %s Documentation Builder" % project_version(path_repo))
    parser.add_argument("--repo-path", default=path_repo, metavar="PATH", help="code repository path")
    parser.add_argument("--doc-path", metavar="PATH", help="documentation path to render (manifest.xml is located here)")
    parser.add_argument("--out", action="append", default=[], metavar="TYPE", help="output type (html, markdown, man)")
    parser.add_argument("--out-preserve", action="store_true", help="do not clean the output path")
    parser.add_argument("--require", action="append", default=[], metavar="SECTION", help="render only certain sections")
    parser.add_argument("--include", action="append", default=[], metavar="SOURCE", help="include a source in the build")
    parser.add_argument("--exclude", action="append", default=[], metavar="SOURCE", help="exclude a source from the build")
    parser.add_argument("--var", action="append", default=[], metavar="KEY=VALUE", help="override a variable")
    parser.add_argument("--key-var", action="append", default=[], metavar="KEY=VALUE", help="override a variable and key the cache")
    parser.add_argument("--deploy", action="store_true", help="write the execution cache into resource for persistence")
    parser.add_argument("--no-exe", dest="exe", action="store_false", help="do not run the commands (for testing only)")
    parser.add_argument("--no-cache", dest="cache", action="store_false", help="do not use the execution cache")
    parser.add_argument("--cache-only", action="store_true", help="only use the execution cache, do not generate it")
    parser.add_argument("--pre", action="store_true", help="pre-build the images for hosts with commands marked pre")
    parser.add_argument("--dev", action="store_true", help="set the dev variable to y")
    parser.add_argument("--debug", action="store_true", help="set the debug variable to y")
    parser.add_argument("--quiet", action="store_true", help="set the log level to error")
    parser.add_argument(
        "--log-level", default="info", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="console log level"
    )
    parser.add_argument("--no-log-timestamp", dest="log_timestamp", action="store_false", help="suppress timestamps in the log")

    config = parser.parse_args(arg_list)

    # A relative path is made absolute so it does not depend on where the tool was run from
    config.repo_path = os.path.abspath(config.repo_path)
    config.doc_path = os.path.join(config.repo_path, "doc") if config.doc_path is None else os.path.abspath(config.doc_path)

    # Nothing is cached when the commands are not run, since there would be nothing to cache
    if not config.exe:
        config.cache = False

    if config.deploy:
        if not config.exe:
            raise ToolError("--no-exe cannot be specified for deploy")

        if len(config.require) > 0:
            raise ToolError("--require cannot be specified for deploy")

    # A partial render is of one document, since the sections it names are sections of that document
    if len(config.require) > 0 and len(config.include) != 1:
        raise ToolError("one --include is required when --require is specified")

    if len(config.include) > 0 and len(config.exclude) > 0:
        raise ToolError("cannot specify both --include and --exclude")

    # Variables that key the cache are variables as well, so a document refers to them the same way
    config.key_var_map = _var_parse(config.key_var)
    config.var_map = _var_parse(config.var)

    for key in sorted(config.key_var_map):
        if key in config.var_map:
            raise ToolError("'%s' cannot be passed as --var and --key-var" % key)

        config.var_map[key] = config.key_var_map[key]

    if config.dev:
        config.var_map["dev"] = "y"

    # The debug variable always says what the flag says, since a document declaring it would otherwise decide for the caller
    config.var_map["debug"] = "y" if config.debug else "n"

    config.log_level = OFF if config.quiet else log_level_parse(config.log_level)

    return config


####################################################################################################################################
def _var_parse(var_list):
    """Parse variables given as key=value, where a value may itself hold an equals sign."""

    result = {}

    for var in var_list:
        key, _, value = var.partition("=")

        if key == "" or "=" not in var:
            raise ToolError("variable '%s' must be given as key=value" % var)

        result[key] = value

    return result


####################################################################################################################################
def _read(path):
    """Read a document and drop what it uses to lay itself out but does not mean."""

    result = xml_document_parse(file_read(path), path)

    xml_node_normalize(result)

    return result


####################################################################################################################################
def _release_date(release_root, static):
    """The date the documentation says it was built, which a release build takes from the release so it can be built again."""

    if static:
        date = xml_node_attribute(release_last(release_root), "date", True)

        if date == "XXXX-XX-XX":
            raise ToolError("not possible to use static release dates on a dev build")
    else:
        now = time.localtime()
        date = "%04d-%02d-%02d" % (now.tm_year, now.tm_mon, now.tm_mday)

    return date_render(date), date[0:4]


####################################################################################################################################
def _build(config, var_store):
    """Build the documents that are generated rather than written, and the manual page."""

    path_repo = config.repo_path
    path_xml = os.path.join(config.doc_path, _PATH_XML)

    bld_cfg = bld_cfg_parse(path_repo)
    bld_hlp = bld_hlp_parse(os.path.join(path_xml, "reference.xml"), bld_cfg, True)

    index = _read(os.path.join(path_xml, "index.xml"))
    news = _read(os.path.join(path_xml, "news.xml"))
    user_guide = _read(os.path.join(path_xml, "user-guide.xml"))
    release = _read(os.path.join(path_xml, "release.xml"))

    # What the documentation says about the project, which is read from the project rather than said again in the documentation
    var_store.add("version", project_version(path_repo))
    var_store.add("version-stable", xml_node_attribute(release_current_stable(release), "version", True))

    date, year = _release_date(release, config.var_map.get("release-date-static", "n") == "y")

    var_store.add("release-date", date)
    var_store.add("release-year", year)

    document_map = {
        "command": reference_command_render(bld_cfg, bld_hlp),
        "configuration": reference_configuration_render(bld_cfg, bld_hlp),
        "index": news_index_render(index, news),
        "news": news_render(news),
        "user-guide": build_pre(user_guide, bld_hlp, var_store),
        "release": release_render(release, config.doc_path, config.var_map.get("dev") == "y"),
    }

    return document_map, reference_man_render(index, bld_cfg, bld_hlp)


####################################################################################################################################
def _host_build(manifest, config):
    """Build the image for every host the documentation defines."""

    # A document that was excluded is not in the source map at all, so only what was included has to be checked here
    for key in sorted(manifest.source_map):
        if len(config.include) > 0 and key not in config.include:
            continue

        log(INFO, "source %s" % key)

        for host in xml_node_child_list(manifest.source_map[key].root, "host-define"):
            if not manifest.evaluate_if(host):
                continue

            replace = manifest.var_store.replace_str
            image = replace(xml_node_attribute(host, "image", True))
            source = replace(xml_node_attribute(host, "from", True))

            log(INFO, "Build vm '%s' from '%s'" % (image, source))

            image_build_cached(
                os.path.join(config.doc_path, _PATH_OUT, "doc-host.dockerfile"),
                image,
                "FROM %s\n\n%s\n" % (source, replace(xml_node_content(host)).strip()),
                config.doc_path,
                xml_node_attribute(host, "revision") or "0",
            )


####################################################################################################################################
def _out_clean(path_out):
    """Empty the output path so what is left in it is what this build wrote."""

    if not os.path.isdir(path_out):
        path_create(path_out)

        return

    for name in path_list(path_out):
        path = os.path.join(path_out, name)

        shutil.rmtree(path) if os.path.isdir(path) else os.remove(path)


####################################################################################################################################
def cmd_doc(config):
    """Build the documentation and render it."""

    var_store = VarStore()

    # What the caller asked for is loaded first so a document declaring the same variable does not override it
    for key in sorted(config.var_map):
        var_store.add(key, config.var_map[key])

    document_map, man = _build(config, var_store)

    manifest = Manifest(config.doc_path, var_store, document_map, config)

    if config.cache:
        manifest.cache_read()

    out_list = config.out

    if len(out_list) == 0:
        out_list = sorted(manifest.render_map)

        # The manual page is of the project rather than of the documentation, so it is only built for the project
        if var_store.test("project-exe", PROJECT_EXE):
            out_list.append(RENDER_MAN)

    if not config.cache_only and config.exe:
        _host_build(manifest, config)

    for out in out_list:
        log(INFO, "render %s output" % out)

        path_out = os.path.join(config.doc_path, _PATH_OUT, out)

        if out == RENDER_MAN:
            file_write(os.path.join(path_out, _FILE_MAN), man)

            continue

        if not config.out_preserve:
            _out_clean(path_out)
        else:
            path_create(path_out)

        manifest.render_get(out)

        if out == RENDER_MARKDOWN:
            markdown_render(manifest, path_out, config.exe)
        else:
            html_render(manifest, config.doc_path, path_out, config.exe)

    # Links between pages can only be checked once every page is rendered and the sections of each are known
    manifest.link_verify()

    if config.cache and not config.cache_only:
        manifest.cache_write()
