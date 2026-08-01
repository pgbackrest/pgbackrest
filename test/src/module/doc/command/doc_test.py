"""Test Documentation Build.

Each renderer has its own test, so what is checked here is that the build reads what it should, writes what it should, and loads the
variables the caller gave before the documents that use them."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from command.doc import *
from command.doc import _out_clean, _release_date, _var_parse
from common.error import *
from common.storage import file_read, file_write, path_list

CONFIG = """command:
  backup: {}

optionGroup:
  repo: {}

option:
  stanza:
    type: string
"""

HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <text><p>Introduction.</p></text>

        <config-section-list/>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <text><p>Introduction.</p></text>

        <operation-general title="General Options">
            <option-list>
                <option id="stanza" name="Stanza">
                    <summary>Stanza name.</summary>

                    <text><p>Description.</p></text>
                </option>
            </option-list>
        </operation-general>

        <command-list>
            <command id="backup" name="Backup">
                <summary>Back up a cluster.</summary>

                <text><p>Description.</p></text>
            </command>
        </command-list>
    </operation>
</doc>
"""

INDEX = """<doc title="{[project]}" subtitle="Reliable Backup">
    <description>{[project]} backs things up.</description>

    <host-define image="pgbackrest/doc:test" from="ubuntu:24.04" revision="20260730A">RUN apt-get update</host-define>
    <host-define image="skipped" from="ubuntu:24.04" if="'{[mode]}' eq 'never'">RUN true</host-define>

    <section id="intro">
        <title>Intro</title>

        <p>Running in {[mode]} mode.</p>
    </section>
</doc>
"""

USER_GUIDE = """<doc title="User Guide">
    <description>How to use it.</description>


    <variable-list>
        <variable key="host">local</variable>
    </variable-list>

    <section id="start">
        <title>Start</title>

        <p>Running on {[host]}.</p>
    </section>
</doc>
"""

RELEASE = """<doc title="{[project]} Releases">
    <description>Releases of {[project]}.</description>

    <intro>
        <text><p>About the releases.</p></text>
    </intro>

    <release-list>
        <release date="2026-07-30" version="2.00" title="Latest">
            <release-core-list>
                <release-improvement-list>
                    <release-item>
                        <p>Make it faster.</p>
                    </release-item>
                </release-improvement-list>
            </release-core-list>
        </release>
    </release-list>

    <contributor-list>
        <contributor id="david.steele">
            <contributor-name-display>David Steele</contributor-name-display>
        </contributor>
    </contributor-list>
</doc>
"""

MANIFEST = """<doc>
    <variable-list>
        <variable key="project">pgBackRest</variable>
        <variable key="project-exe">pgbackrest</variable>
        <variable key="project-url-root">/</variable>
        <variable key="project-logo">logo.png</variable>
        <variable key="project-favicon">logo.svg</variable>
        <variable key="html-footer">Updated {[release-date]}</variable>
        <variable key="mode">release</variable>
    </variable-list>

    <source-list>
        <source key="index"/>
        <source key="user-guide"/>
        <source key="command"/>
        <source key="configuration"/>
        <source key="release"/>
    </source-list>

    <render-list>
        <render type="html">
            <render-source key="index" menu="Home"/>
            <render-source key="user-guide"/>
        </render>

        <render type="markdown">
            <render-source key="index" file="../../../README.md"/>
        </render>
    </render-list>
</doc>
"""

HISTORY = """[{"commit": "abc", "date": "2026-07-30 00:00:00 +0000", "subject": "Make it faster."}]
"""

VERSION_H = """#define PROJECT_VERSION_MAJOR                                       2
#define PROJECT_VERSION_MINOR                                       60
#define PROJECT_VERSION_PATCH                                       0
#define PROJECT_VERSION_SUFFIX                                      ""
"""


####################################################################################################################################
def _repo(path):
    """Write the documentation a build reads, which is the smallest one that exercises every part of it."""

    file_write(os.path.join(path, "src/version.h"), VERSION_H)
    file_write(os.path.join(path, "build/config.yaml"), CONFIG)
    file_write(os.path.join(path, "doc/manifest.xml"), MANIFEST)
    file_write(os.path.join(path, "doc/xml/index.xml"), INDEX)
    file_write(os.path.join(path, "doc/xml/reference.xml"), HELP)
    file_write(os.path.join(path, "doc/xml/user-guide.xml"), USER_GUIDE)
    file_write(os.path.join(path, "doc/xml/release.xml"), RELEASE)
    file_write(os.path.join(path, "doc/resource/git-history.cache"), HISTORY)
    file_write(os.path.join(path, "doc/resource/html/default.css"), "body { color: black; }\n")
    file_write(os.path.join(path, "doc/resource/html/default.js"), "// script\n")
    file_write(os.path.join(path, "doc/resource/slogo.svg"), "<svg></svg>\n")
    file_write(os.path.join(path, "doc/resource/logo.png"), "png")
    file_write(os.path.join(path, "doc/resource/logo.svg"), "svg")
    file_write(os.path.join(path, "doc/resource/sponsor/one.png"), "png")


####################################################################################################################################
def test_cfg_load():
    """A command line is parsed and what cannot be expressed in the parser is applied."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "src/version.h"), VERSION_H)

        config = cfg_load([], path)

        assert_equal(config.repo_path, path)
        assert_equal(config.doc_path, os.path.join(path, "doc"))
        assert_equal(config.var_map, {"debug": "n"})
        assert_true(config.exe)
        assert_true(config.cache)

        # Nothing is cached when the commands are not run, since there would be nothing to cache
        assert_false(cfg_load(["--no-exe"], path).cache)

        # Quiet is a shorthand for turning the log off
        assert_equal(cfg_load(["--quiet"], path).log_level, OFF)

        # A relative path is made absolute so it does not depend on where the tool was run from
        assert_equal(cfg_load(["--doc-path=doc"], path).doc_path, os.path.join(os.getcwd(), "doc"))

        # The dev and debug flags are variables, and debug always says what the flag says
        config = cfg_load(["--dev", "--debug"], path)

        assert_equal(config.var_map["dev"], "y")
        assert_equal(config.var_map["debug"], "y")


####################################################################################################################################
def test_cfg_load_error():
    """A combination of options that cannot work is reported rather than doing part of what was asked."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "src/version.h"), VERSION_H)

        for arg_list, message in (
            (["--deploy", "--no-exe"], "--no-exe cannot be specified for deploy"),
            (["--deploy", "--require=/x", "--include=a"], "--require cannot be specified for deploy"),
            (["--require=/x"], "one --include is required when --require is specified"),
            (["--require=/x", "--include=a", "--include=b"], "one --include is required when --require is specified"),
            (["--include=a", "--exclude=b"], "cannot specify both --include and --exclude"),
            (["--var=x", "--key-var=y=1"], "variable 'x' must be given as key=value"),
            (["--var=x=1", "--key-var=x=1"], "'x' cannot be passed as --var and --key-var"),
        ):
            with assert_raises(ToolError) as raised:
                cfg_load(arg_list, path)

            assert_equal(str(raised.exception), message)


####################################################################################################################################
def test_var_parse():
    """Variables are given as key=value, and a value may itself hold an equals sign or nothing at all."""

    assert_equal(_var_parse(["a=1", "b=x=y", "c="]), {"a": "1", "b": "x=y", "c": ""})

    with assert_raises(ToolError) as raised:
        _var_parse(["=1"])

    assert_equal(str(raised.exception), "variable '=1' must be given as key=value")


####################################################################################################################################
def test_release_date():
    """A release build takes the date from the release so the documentation can be built again and come out the same."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "release.xml"), RELEASE)

        from common.xml import xml_document_parse

        release = xml_document_parse(file_read(os.path.join(path, "release.xml")), path)

        assert_equal(_release_date(release, True), ("July 30, 2026", "2026"))

        # A build that is not of a release is dated today
        date, year = _release_date(release, False)

        assert_in(year, date)

        # A release that has no date yet cannot be built as though it did
        release = xml_document_parse(RELEASE.replace('date="2026-07-30"', 'date="XXXX-XX-XX"'), path)

        with assert_raises(ToolError) as raised:
            _release_date(release, True)

        assert_equal(str(raised.exception), "not possible to use static release dates on a dev build")


####################################################################################################################################
def test_out_clean():
    """The output path holds what this build wrote rather than what a previous one left behind."""

    with tempfile.TemporaryDirectory() as path:
        path_out = os.path.join(path, "out")

        # A path that is not there yet is created rather than cleaned
        _out_clean(path_out)

        assert_equal(path_list(path_out), [])

        file_write(os.path.join(path_out, "old.html"), "old")
        file_write(os.path.join(path_out, "sponsor/logo.png"), "old")

        _out_clean(path_out)

        assert_equal(path_list(path_out), [])


####################################################################################################################################
def test_doc():
    """The documentation is built and rendered, with what the caller asked for winning over what a document declares."""

    with tempfile.TemporaryDirectory() as path:
        _repo(path)

        cmd_doc(cfg_load(["--no-exe", "--repo-path=%s" % path], path))

        path_out = os.path.join(path, "doc/output")

        # Every render type the manifest declares is rendered, along with the manual page since this is the project
        assert_equal(sorted(path_list(path_out)), ["html", "man", "markdown"])
        assert_equal(
            sorted(path_list(os.path.join(path_out, "html"))),
            ["default.css", "default.js", "index.html", "logo.png", "logo.svg", "slogo.svg", "sponsor", "user-guide.html"],
        )

        # What goes beside the pages is copied rather than rendered
        assert_equal(path_list(os.path.join(path_out, "html/sponsor")), ["one.png"])

        # A document is rendered from what was built rather than from a file, so nothing is left behind to go stale
        index = file_read(os.path.join(path_out, "html/index.html"))

        assert_in("<title>", index)
        assert_in("Running in release mode.", index)
        assert_in("Updated ", index)

        # The user guide resolves the variables it declares
        assert_in("Running on local.", file_read(os.path.join(path_out, "html/user-guide.html")))

        # The manual page is written where the distribution takes it from
        assert_in("pgBackRest - Reliable Backup", file_read(os.path.join(path_out, "man/pgbackrest.1.txt")))

        # Markdown is written where the manifest says, which for the readme is the root of the repository
        assert_in("# pgBackRest", file_read(os.path.join(path, "README.md")))


####################################################################################################################################
def test_doc_var():
    """A variable the caller gives is not overridden by a document declaring it."""

    with tempfile.TemporaryDirectory() as path:
        _repo(path)

        cmd_doc(cfg_load(["--no-exe", "--repo-path=%s" % path, "--var=mode=debug", "--out=html", "--include=index"], path))

        assert_in("Running in debug mode.", file_read(os.path.join(path, "doc/output/html/index.html")))

        # Only what was included is rendered
        assert_not_in("user-guide.html", path_list(os.path.join(path, "doc/output/html")))


####################################################################################################################################
def test_doc_key_var():
    """A variable that keys the cache is a variable as well, so a document refers to it the same way."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "src/version.h"), VERSION_H)

        config = cfg_load(["--key-var=os-type=rhel"], path)

        assert_equal(config.key_var_map, {"os-type": "rhel"})
        assert_equal(config.var_map["os-type"], "rhel")


####################################################################################################################################
def test_doc_host_build():
    """The image for every host the documentation defines is built before anything runs on it."""

    import command.doc as doc_module

    build_list = []

    def _image_build_cached(path_docker_file, image, script, path_context, revision):
        build_list.append((image, script, revision))

    image_build_cached_real = doc_module.image_build_cached
    doc_module.image_build_cached = _image_build_cached

    try:
        with tempfile.TemporaryDirectory() as path:
            _repo(path)

            # The cache is not used, so the images are built even though nothing is run against them
            cmd_doc(cfg_load(["--repo-path=%s" % path, "--out=man"], path))

            # A host whose condition does not hold is not built
            assert_equal(build_list, [("pgbackrest/doc:test", "FROM ubuntu:24.04\n\nRUN apt-get update\n", "20260730A")])

            # A document that is not part of this build has no hosts to build either
            build_list.clear()

            cmd_doc(cfg_load(["--repo-path=%s" % path, "--out=man", "--include=user-guide"], path))

            assert_equal(build_list, [])
    finally:
        doc_module.image_build_cached = image_build_cached_real


####################################################################################################################################
def test_doc_out_preserve():
    """A render that preserves the output path adds to what is there rather than replacing it."""

    with tempfile.TemporaryDirectory() as path:
        _repo(path)

        path_html = os.path.join(path, "doc/output/html")

        file_write(os.path.join(path_html, "keep.html"), "keep")

        cmd_doc(cfg_load(["--no-exe", "--repo-path=%s" % path, "--out=html", "--out-preserve"], path))

        assert_in("keep.html", path_list(path_html))
        assert_in("index.html", path_list(path_html))


####################################################################################################################################
def test_doc_deploy():
    """A deploy writes the cache where it is kept, and only builds the manual page for the project itself."""

    with tempfile.TemporaryDirectory() as path:
        _repo(path)

        # A documentation that is not of the project has no manual page of the project to build
        cmd_doc(cfg_load(["--repo-path=%s" % path, "--deploy", "--cache-only", "--var=project-exe=other"], path))

        assert_equal(sorted(path_list(os.path.join(path, "doc/output"))), ["html", "markdown"])


####################################################################################################################################
def test_doc_render_invalid():
    """A render type the tool does not know is reported rather than rendered as something else."""

    with tempfile.TemporaryDirectory() as path:
        _repo(path)

        file_write(os.path.join(path, "doc/manifest.xml"), MANIFEST.replace('type="markdown"', 'type="pdf"'))

        with assert_raises(ToolError) as raised:
            cmd_doc(cfg_load(["--no-exe", "--repo-path=%s" % path], path))

        assert_equal(str(raised.exception), "render type 'pdf' is not valid")
