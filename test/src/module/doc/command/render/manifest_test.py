"""Test Documentation Manifest."""

####################################################################################################################################
import json
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_read, file_write
from common.var_store import VarStore
from common.xml import xml_node_attribute, xml_parse
from command.render.manifest import RENDER_HTML, RENDER_MARKDOWN, Manifest

MANIFEST = """<doc>
    <variable-list>
        <variable key="project">pgBackRest</variable>
        <variable key="title">{[project]} Guide</variable>
        <variable key="news">n</variable>
        <variable key="skipped" if="'{[news]}' eq 'y'">yes</variable>
    </variable-list>

    <source-list>
        <source key="index"/>
        <source key="user-guide"/>
        <source key="news"/>
    </source-list>

    <render-list>
        <render type="html">
            <render-source key="index" menu="Home"/>
            <render-source key="user-guide"/>
            <render-source key="news"/>
        </render>

        <render type="markdown">
            <render-source key="index" file="../../../README.md"/>
        </render>
    </render-list>
</doc>
"""

INDEX = """<doc title="Index">
    <variable-list>
        <variable key="index-var">from the index</variable>
    </variable-list>
</doc>
"""


####################################################################################################################################
class _Config:
    """The options a manifest reads, which is a few of what the tool takes."""

    def __init__(self, **kwargs):
        self.deploy = False
        self.cache_only = False
        self.pre = False
        self.require = []
        self.include = []
        self.exclude = []
        self.key_var_map = {}

        for name, value in kwargs.items():
            setattr(self, name, value)


####################################################################################################################################
def _doc_path(path, manifest=MANIFEST):
    """Write the documentation a manifest reads."""

    file_write(os.path.join(path, "manifest.xml"), manifest)
    file_write(os.path.join(path, "xml/index.xml"), INDEX)
    file_write(os.path.join(path, "xml/news.xml"), '<doc title="News"/>')

    return path


####################################################################################################################################
def _manifest(path, config=None, document_map=None):
    """Load a manifest, with the user guide standing in for a document the tool builds rather than reads."""

    return Manifest(
        path,
        VarStore(),
        {"user-guide": xml_parse('<doc title="User Guide"/>', "test.xml")} if document_map is None else document_map,
        _Config() if config is None else config,
    )


####################################################################################################################################
def test_manifest():
    """A manifest says what the documentation is made of and what it is rendered to."""

    with tempfile.TemporaryDirectory() as path:
        manifest = _manifest(_doc_path(path))

        # A document is built when the tool builds it and read from doc/xml when it does not
        assert_equal(sorted(manifest.source_map), ["index", "news", "user-guide"])
        assert_equal(xml_node_attribute(manifest.source_get("index").root, "title"), "Index")
        assert_equal(xml_node_attribute(manifest.source_get("user-guide").root, "title"), "User Guide")

        # Variables a document declares are loaded as the document is, and the doc path is set before the manifest declares its own
        assert_equal(manifest.var_store.get("index-var"), "from the index")
        assert_equal(manifest.var_store.get("doc-path"), path)

        # A value may itself refer to a variable
        assert_equal(manifest.var_store.get("title"), "pgBackRest Guide")

        # A variable whose condition does not hold is not declared at all
        assert_is_none(manifest.var_store.get("skipped"))

        with assert_raises(ToolError) as raised:
            manifest.source_get("missing")

        assert_equal(str(raised.exception), "source 'missing' does not exist")


####################################################################################################################################
def test_manifest_render():
    """A render type says how it is written and which documents are rendered to it."""

    with tempfile.TemporaryDirectory() as path:
        manifest = _manifest(_doc_path(path))

        html = manifest.render_get(RENDER_HTML)

        assert_true(html.menu)

        # The order is the order the manifest lists the pages in, which is the order the menu is in
        assert_equal(html.order, ["index", "user-guide", "news"])
        assert_equal(manifest.render_out_get(RENDER_HTML, "index").menu, "Home")
        assert_is_none(manifest.render_out_get(RENDER_HTML, "user-guide").menu)

        markdown = manifest.render_get(RENDER_MARKDOWN)

        assert_false(markdown.menu)
        assert_equal(markdown.out_map["index"].file, "../../../README.md")

        # A page that is not part of this build has nothing to point at
        assert_is_none(manifest.render_out_get(RENDER_HTML, "missing"))
        assert_is_none(manifest.render_out_get("man", "index"))

        with assert_raises(ToolError) as raised:
            manifest.render_get("man")

        assert_equal(str(raised.exception), "render type 'man' does not exist")


####################################################################################################################################
def test_manifest_error():
    """A manifest that says something twice or says something the tool does not know is reported."""

    with tempfile.TemporaryDirectory() as path:
        with assert_raises(ToolError) as raised:
            _manifest(_doc_path(path, MANIFEST.replace('<render type="markdown">', '<render type="html">')))

        assert_equal(str(raised.exception), "render 'html' has already been defined")

        with assert_raises(ToolError) as raised:
            _manifest(_doc_path(path, MANIFEST.replace('type="markdown"', 'type="pdf"')))

        assert_equal(str(raised.exception), "render type 'pdf' is not valid")

        with assert_raises(ToolError) as raised:
            _manifest(
                _doc_path(
                    path,
                    MANIFEST.replace(
                        '<render-source key="index" file="../../../README.md"/>',
                        '<render-source key="index" file="../../../README.md" menu="Home"/>',
                    ),
                )
            )

        assert_equal(str(raised.exception), "menu is only valid with the html render type")


####################################################################################################################################
def test_manifest_include():
    """A build of part of the documentation renders only what was asked for."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        # A document that is excluded is not part of the documentation at all
        manifest = _manifest(path, _Config(exclude=["news"]))

        assert_equal(sorted(manifest.source_map), ["index", "user-guide"])
        assert_equal(sorted(manifest.render_get(RENDER_HTML).out_map), ["index", "user-guide"])

        # A document that is not included is still there, it is just not rendered
        manifest = _manifest(path, _Config(include=["index"]))

        assert_equal(sorted(manifest.source_map), ["index", "news", "user-guide"])
        assert_equal(sorted(manifest.render_get(RENDER_HTML).out_map), ["index"])


####################################################################################################################################
def test_manifest_link_verify():
    """A link to a section of a page is checked once every page is rendered and the sections of each are known."""

    with tempfile.TemporaryDirectory() as path:
        manifest = _manifest(_doc_path(path))

        manifest.page_anchor_map["index"] = {"setup": True}
        manifest.link_list.append({"source": "user-guide", "page": "index", "section": "/setup"})

        manifest.link_verify()

        # A link to a page that was not rendered in this build has nothing to check it against
        manifest.link_list.append({"source": "user-guide", "page": "other", "section": "/missing"})

        manifest.link_verify()

        manifest.link_list.append({"source": "user-guide", "page": "index", "section": "/missing"})

        with assert_raises(ToolError) as raised:
            manifest.link_verify()

        assert_equal(str(raised.exception), "page 'user-guide' has a link to page 'index' section '/missing' that does not exist")


####################################################################################################################################
def test_manifest_cache():
    """The cache is what a command wrote, kept so the documentation can be rendered again without running any of it."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        cache = {"default": {"all": {"index": [{"key": {"host": "repo"}, "type": "exe"}]}}}

        file_write(os.path.join(path, "output/exe.cache"), json.dumps(cache))

        manifest = _manifest(path)
        manifest.cache_read()

        assert_equal(manifest.source_map["index"].cache_list, cache["default"]["all"]["index"])
        assert_is_none(manifest.source_map["news"].cache_list)

        # Writing it back keeps what it holds for builds other than this one
        manifest.source_map["news"].cache_list = [{"key": {"host": "pg"}, "type": "exe"}]
        manifest.cache_write()

        written = json.loads(file_read(os.path.join(path, "output/exe.cache")))

        assert_equal(sorted(written["default"]["all"]), ["index", "news"])


####################################################################################################################################
def test_manifest_cache_key():
    """The cache is keyed by everything that changes what the commands would be."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        cache = {'{"os-type":"rhel"}': {"/quickstart": {"index": [{"key": {}, "type": "exe"}]}}}

        file_write(os.path.join(path, "resource/exe.cache"), json.dumps(cache))

        # A build for a different platform or of a different part of the documentation is a different cache
        manifest = _manifest(path, _Config(key_var_map={"os-type": "rhel"}, require=["/quickstart"], deploy=True))
        manifest.cache_read()

        assert_is_not_none(manifest.source_map["index"].cache_list)

        manifest = _manifest(path, _Config(key_var_map={"os-type": "debian"}, require=["/quickstart"], deploy=True))
        manifest.cache_read()

        assert_is_none(manifest.source_map["index"].cache_list)

        # The deploy copy is what a build reads when there is no local copy to prefer
        manifest = _manifest(path, _Config(key_var_map={"os-type": "rhel"}, require=["/quickstart"]))
        manifest.cache_read()

        assert_is_not_none(manifest.source_map["index"].cache_list)


####################################################################################################################################
def test_manifest_cache_missing():
    """A build with no cache to read builds everything, and writes nothing when there was nothing to cache."""

    with tempfile.TemporaryDirectory() as path:
        manifest = _manifest(_doc_path(path))

        manifest.cache_read()
        manifest.cache_write()

        assert_false(os.path.exists(os.path.join(path, "output/exe.cache")))


####################################################################################################################################
def test_manifest_cache_reset():
    """A cache that no longer describes a document is thrown away, unless the caller said to use the cache and nothing else."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        manifest = _manifest(path)
        manifest.source_map["index"].cache_list = [{"key": {}, "type": "exe"}]

        manifest.cache_reset("index")

        assert_is_none(manifest.source_map["index"].cache_list)

        manifest = _manifest(path, _Config(cache_only=True))

        with assert_raises(ToolError) as raised:
            manifest.cache_reset("index")

        assert_equal(str(raised.exception), "cache reset disabled by --cache-only option")


####################################################################################################################################
def test_manifest_cache_format():
    """The cache is written the way it has always been written, so regenerating it shows only what changed in the documentation."""

    with tempfile.TemporaryDirectory() as path:
        _doc_path(path)

        # What the tool that wrote the cache before this one produced, down to the space before each colon
        content = (
            "{\n"
            '   "default" : {\n'
            '      "all" : {\n'
            '         "index" : [\n'
            "            {\n"
            '               "key" : {\n'
            '                  "host" : "repo"\n'
            "               },\n"
            '               "type" : "exe"\n'
            "            }\n"
            "         ]\n"
            "      }\n"
            "   }\n"
            "}\n"
        )

        file_write(os.path.join(path, "output/exe.cache"), content)

        manifest = _manifest(path)
        manifest.cache_read()
        manifest.cache_write()

        assert_equal(file_read(os.path.join(path, "output/exe.cache")), content)
