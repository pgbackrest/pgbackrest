#!/usr/bin/env python3
"""pgBackRest Release Manager.

Builds the documentation for a release, deploys it to the website, and builds the copy that ships in the distribution. A separate
program from the documentation builder because it is about a release rather than about a document: it decides which documentation to
build, for which platform, and where it goes.

The documentation is built for each platform it documents, since the commands a reader runs differ between them, and the copy of the
user guide for each is kept under a name of its own.

All output, including errors, goes to stdout so the run reads in the order it happened."""

####################################################################################################################################
import argparse
import os
import re
import shutil
import signal
import sys

# Send everything written to stderr to stdout instead so the output is in the order it happened
sys.stderr = sys.stdout

# Do not cache bytecode. The tool runs from the source tree, where a __pycache__ would show up as an unexpected binary file in the
# linter and in the distribution. This must be set before the library modules are imported below.
sys.dont_write_bytecode = True

# Each tool keeps its library beside itself and may use the libraries below it in the hierarchy. Insert them first, lowest last, so
# the doc modules are found before anything else on the path.
for lib in ("build", "doc"):
    sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), lib, "lib"))

from command.render.release import release_last  # noqa: E402
from common.error import EXIT_ERROR, EXIT_TERM, ToolError, error_trace  # noqa: E402
from common.exec import exec_result  # noqa: E402
from common.log import *  # noqa: E402
from common.storage import file_read, file_remove, path_list_recurse  # noqa: E402
from common.xml import xml_document_parse, xml_node_attribute, xml_node_normalize  # noqa: E402
from config.project import PROJECT_NAME, project_version  # noqa: E402

# Where the documentation is built and where the release keeps what it needs
_FILE_CACHE = "resource/exe.cache"
_PATH_HTML = "output/html"
_PATH_SITE = "site"

# Files at the top of the site that belong to the site rather than to a build of the documentation, so a deploy leaves them alone.
# CNAME is what points the domain at the site, so removing it takes the site off its domain until someone puts it back.
_SITE_KEEP_LIST = ("CNAME",)

# The footer of a page, and a release date as date_render() writes it or a release year within the footer
_FOOTER_EXP = re.compile(r'<div class="page-footer">.*?</div>', re.DOTALL)
_FOOTER_DATE_EXP = re.compile(r"[A-Z][a-z]+ [0-9]{1,2}, [0-9]{4}|[0-9]{4}")

# The distributions the documentation is built for, named for the os type the documentation generates commands for. The user guide
# for each is kept under a name of its own, since a reader picks the one for the system they are on.
_DISTRO_LIST = ("debian", "rhel")

_FILE_USER_GUIDE = "user-guide.html"
_FILE_USER_GUIDE_RHEL = "user-guide-rhel.html"


####################################################################################################################################
def cfg_load(arg_list, path_repo):
    """Parse the command line and apply the rules that cannot be expressed in the parser."""

    parser = argparse.ArgumentParser(prog="release.py", description="pgBackRest Release Manager")

    parser.add_argument("--version", action="version", version="pgBackRest %s Release Manager" % project_version(path_repo))
    parser.add_argument("--build", action="store_true", help="build the cache before release (include in the release commit)")
    parser.add_argument("--deploy", action="store_true", help="deploy the documentation to the website")
    parser.add_argument("--dist", action="store_true", help="build the documentation that ships in the distribution")
    parser.add_argument("--no-gen", dest="gen", action="store_false", help="do not regenerate the coverage summary")
    parser.add_argument("--no-exe", dest="exe", action="store_false", help="do not run the commands (only applies to --dist)")
    parser.add_argument(
        "--distro", choices=_DISTRO_LIST, help="build the documentation for one distribution rather than for every one"
    )
    parser.add_argument("--quiet", action="store_true", help="set the log level to error")
    parser.add_argument(
        "--log-level", default="info", choices=sorted(LEVEL_NAME.values()), metavar="LEVEL", help="console log level"
    )

    config = parser.parse_args(arg_list)
    config.repo_path = path_repo
    config.doc_path = os.path.join(path_repo, "doc")

    if not (config.build or config.deploy or config.dist):
        raise ToolError("neither --build nor --deploy nor --dist requested, nothing to do")

    config.log_level = OFF if config.quiet else log_level_parse(config.log_level)

    return config


####################################################################################################################################
def doc_build(config, arg_list, show_output=False):
    """Build the documentation."""

    exec_result(
        "%s %s" % (os.path.join(config.doc_path, "doc.py"), " ".join(arg_list)),
        suppress_stderr=True,
        show_output=show_output,
    )


####################################################################################################################################
def user_guide_rename(config):
    """Keep the user guide that was just built under a name of its own, so the next one does not replace it."""

    path_html = os.path.join(config.doc_path, _PATH_HTML)

    os.replace(os.path.join(path_html, _FILE_USER_GUIDE), os.path.join(path_html, _FILE_USER_GUIDE_RHEL))


####################################################################################################################################
def cmd_build(config):
    """Build the documentation for a release, along with the cache that lets it be built again without running anything."""

    if config.gen:
        log(INFO, "generate coverage summary")

        exec_result(
            "%s --vm=u24 --no-valgrind --clean --coverage-summary" % os.path.join(config.repo_path, "test/test.py"),
            suppress_stderr=True,
            show_output=True,
        )

    # The cache is built from nothing, since what it holds is what this release does rather than what the last one did
    file_remove(os.path.join(config.doc_path, _FILE_CACHE))

    # Remove the hosts of any previous build so the addresses they are given are the same every time
    host_remove()

    if config.distro is None or config.distro == "rhel":
        log(INFO, "generate RHEL documentation")

        doc_build(config, ["--deploy", "--key-var=os-type=rhel", "--out=html"], show_output=True)

    if config.distro is None or config.distro == "debian":
        log(INFO, "generate Debian/Ubuntu documentation")

        doc_build(config, ["--deploy", "--out=man", "--out=html", "--out=markdown"], show_output=True)

    # A copy of everything for review, which is the documentation as the website will have it
    if config.distro is None:
        log(INFO, "generate full documentation for review")

        doc_build(
            config,
            [
                "--deploy",
                "--out-preserve",
                "--cache-only",
                "--key-var=os-type=rhel",
                "--out=html",
                "--var=project-url-root=index.html",
            ],
        )
        user_guide_rename(config)

        doc_build(
            config, ["--deploy", "--out-preserve", "--cache-only", "--out=man", "--out=html", "--var=project-url-root=index.html"]
        )


####################################################################################################################################
def host_remove():
    """Remove the hosts of a documentation build.

    The addresses hosts are given depend on what else is running, and the documentation shows those addresses, so a build starts
    from nothing."""

    name_list = exec_result("docker ps -a --format '{{.Names}}'", suppress_stderr=True)[1].split()
    doc_list = [name for name in name_list if name.startswith("doc-")]

    if len(doc_list) > 0:
        exec_result("docker rm -f %s" % " ".join(doc_list), suppress_error=True)


####################################################################################################################################
def footer_date_remove(content):
    """Remove the release date and year from the footer of a page."""

    return _FOOTER_EXP.sub(lambda match: _FOOTER_DATE_EXP.sub("", match.group(0)), content)


####################################################################################################################################
def site_file_copy(source, target):
    """Copy a file to the site unless it is a page that differs from the copy on the site only in the release date and year."""

    if not (
        source.endswith(".html")
        and os.path.isfile(target)
        and footer_date_remove(file_read(source)) == footer_date_remove(file_read(target))
    ):
        shutil.copy2(source, target)


####################################################################################################################################
def cmd_deploy(config, dev):
    """Deploy the documentation to the website."""

    path_html = os.path.join(config.doc_path, _PATH_HTML)
    path_site = os.path.join(config.doc_path, _PATH_SITE)
    version = "dev" if dev else project_version(config.repo_path)

    log(INFO, "generate website %s documentation" % ("dev" if dev else "history"))

    # A development build is rendered as it is, and a release is rendered from the cache so it comes out the same every time
    arg_list = (["--dev"] if dev else ["--deploy", "--cache-only"]) + ["--var=project-url-root=index.html", "--out=html"]

    doc_build(config, arg_list + ["--out-preserve", "--key-var=os-type=rhel"])
    user_guide_rename(config)

    file_remove(os.path.join(path_html, "release.html"))
    doc_build(config, arg_list + ["--out-preserve", "--exclude=release"])

    log(INFO, "...deploy to repository")

    path_prior = os.path.join(path_site, "prior", version)

    # Remove files the build did not write
    for name in path_list_recurse(path_prior):
        if not os.path.isfile(os.path.join(path_html, name)):
            file_remove(os.path.join(path_prior, name))

    shutil.copytree(path_html, path_prior, dirs_exist_ok=True, copy_function=site_file_copy)

    # The main website is only replaced by a release, since a development build is of what is not out yet
    if not dev:
        log(INFO, "generate website documentation")

        doc_build(config, ["--var=analytics=y", "--deploy", "--cache-only", "--key-var=os-type=rhel", "--out=html"])
        user_guide_rename(config)
        doc_build(config, ["--var=analytics=y", "--deploy", "--out-preserve", "--cache-only", "--out=html"])

        log(INFO, "...deploy to repository")

        shutil.rmtree(os.path.join(path_site, "dev"), ignore_errors=True)

        for name in os.listdir(path_site):
            path = os.path.join(path_site, name)

            # Only what this build writes, which is regular files the site does not own. A link is the site's too, e.g. a retired
            # platform name pointed at the guide that replaced it, so it is left where it is. A file the build wrote again is not
            # removed.
            if (
                name not in _SITE_KEEP_LIST
                and os.path.isfile(path)
                and not os.path.islink(path)
                and not os.path.isfile(os.path.join(path_html, name))
            ):
                os.remove(path)

        shutil.copytree(path_html, path_site, dirs_exist_ok=True, copy_function=site_file_copy)

        for name in ("README.md", "LICENSE"):
            shutil.copyfile(os.path.join(config.repo_path, name), os.path.join(path_site, name))

    exec_result("find %s -path .git -prune -type d -exec chmod 750 {} +" % path_site)
    exec_result("find %s -path .git -prune -type f -exec chmod 640 {} +" % path_site)


####################################################################################################################################
def cmd_dist(config, dev):
    """Build the documentation that ships in the distribution.

    What ships is the documentation of the project rather than of the website, so what is only of interest on the website is left
    out."""

    log(INFO, "generate dist documentation")

    arg_list = [
        "--var=project-url-root=index.html",
        "--exclude=metric",
        "--exclude=news",
        "--var=card=n",
        "--var=news=n",
        "--var=sponsor=n",
    ]

    if not config.exe:
        arg_list.append("--no-exe")
    elif not dev:
        arg_list += ["--cache-only", "--var=release-date-static=y"]

    doc_build(config, arg_list + ["--key-var=os-type=rhel", "--out=html"])
    user_guide_rename(config)
    doc_build(config, arg_list + ["--out-preserve", "--out=html", "--out=man"])

    # The readme the distribution tarball ships is generated rather than committed, so it is rendered on its own
    doc_build(config, ["--out=markdown", "--include=distribution"])


####################################################################################################################################
def command_run(config):
    """Do what was asked for."""

    version = project_version(config.repo_path)
    dev = version.endswith("dev")

    # The version the code says it is must be the version the release notes say is most recent, since the documentation reports it
    log(INFO, "check version info")

    path_release = os.path.join(config.doc_path, "xml/release.xml")
    release = xml_document_parse(file_read(path_release), path_release)

    xml_node_normalize(release)

    if xml_node_attribute(release_last(release), "version", True) != version:
        raise ToolError("unable to find version %s as the most recent release in %s" % (version, path_release))

    if config.build:
        cmd_build(config)

    if config.deploy:
        cmd_deploy(config, dev)

    if config.dist:
        cmd_dist(config, dev)

    log(INFO, "release complete")

    return 0


####################################################################################################################################
def main():
    """Main."""

    # Die silently on SIGPIPE as C programs do, rather than raising when output is piped to a command that exits early
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)

    path_repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    # Load the configuration. Logging is not initialized until this succeeds so print any error directly.
    try:
        config = cfg_load(sys.argv[1:], path_repo)
    except ToolError as error:
        print(error)

        return EXIT_ERROR

    log_init(config.log_level, True)
    log(INFO, "%s %s Release Manager" % (PROJECT_NAME, project_version(path_repo)))

    try:
        return command_run(config)
    except KeyboardInterrupt:
        # A ctrl-c is what was asked for, so report it the way the C reports a signal rather than as a stack trace
        log(ERROR, "terminated on signal SIGINT")

        return EXIT_TERM
    except ToolError as error:
        log(ERROR, error)

        return error.status
    except Exception as error:
        # An unexpected exception is a bug here rather than a problem with the release, so show the stack trace
        log(ERROR, error_trace(error, config.log_level >= DEBUG))

        return EXIT_ERROR


if __name__ == "__main__":
    sys.exit(main())
