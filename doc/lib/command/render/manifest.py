"""Documentation Manifest.

Reads manifest.xml, which is what says the documentation exists: the documents it is made of, the outputs they are rendered to, and
the variables they all share. Everything a renderer needs to know about the documentation as a whole is here, and everything about
one document is in the document.

It also holds the execution cache. Building the user guide runs real commands against real hosts, which takes long enough that the
result is kept so the documentation can be rendered again without running any of it. The cache is a list per document, replayed in
the order the commands were run, and a key that does not match the command about to run means the cache no longer describes this
document and the whole thing has to be built again."""

####################################################################################################################################
import json
import os

from common.error import ToolError
from common.eval import eval_expression
from common.log import *
from common.storage import file_read, file_write
from common.xml import (
    xml_document_parse,
    xml_node_attribute,
    xml_node_child,
    xml_node_child_list,
    xml_node_content,
)

# Render types, i.e. what a document can be rendered to
RENDER_HTML = "html"
RENDER_MAN = "man"
RENDER_MARKDOWN = "markdown"

# Where the cache lives. The deploy copy is committed so a release can be rendered from it without running anything, and the local
# copy is what a build writes while it is being worked on.
_FILE_CACHE_DEPLOY = "resource/exe.cache"
_FILE_CACHE_LOCAL = "output/exe.cache"


####################################################################################################################################
class Source:
    """A document the documentation is made of."""

    def __init__(self, key, root):
        self.key = key
        self.root = root
        self.cache_list = None  # Commands and their output, when the cache holds this document


####################################################################################################################################
class RenderOut:
    """One output of one render type, i.e. a page."""

    def __init__(self, key, source, file, menu):
        self.key = key
        self.source = source  # Document the page is rendered from, which is the key unless it says otherwise
        self.file = file  # File to write, when it is not named after the key
        self.menu = menu  # Caption in the menu, when the page is in it


####################################################################################################################################
class Render:
    """One render type and everything rendered to it."""

    def __init__(self, type):
        self.type = type
        self.menu = False  # Does any page have a menu caption?
        self.order = []  # Page keys in the order the manifest lists them, which is the order the menu is in
        self.out_map = {}


####################################################################################################################################
class Manifest:
    """What the documentation is made of."""

    def __init__(self, path_doc, var_store, document_map, config):
        self.path_doc = path_doc
        self.var_store = var_store
        self.deploy = config.deploy
        self.cache_only = config.cache_only
        self.pre = config.pre
        self.require_list = config.require
        self.key_var_map = config.key_var_map

        self.source_map = {}
        self.render_map = {}

        # Page anchors and the links to them, collected while rendering so the links can be checked once every page is rendered
        self.page_anchor_map = {}
        self.link_list = []

        self.cache = None

        path_manifest = os.path.join(path_doc, "manifest.xml")
        manifest = xml_document_parse(file_read(path_manifest), path_manifest)

        self._source_load(manifest, document_map, config)
        self._render_load(manifest, config)

        # The doc path is set before the manifest declares its own variables since they refer to it
        var_store.add("doc-path", path_doc)

        self._variable_load(xml_node_child(manifest, "variable-list"))

    ################################################################################################################################
    def _source_load(self, manifest, document_map, config):
        """Load every document, which is built when the tool builds it and read from doc/xml when it does not."""

        for source in xml_node_child_list(xml_node_child(manifest, "source-list", True), "source"):
            key = xml_node_attribute(source, "key", True)

            if key in config.exclude:
                continue

            if key in document_map:
                root = document_map[key]
            else:
                path = os.path.join(self.path_doc, "xml", "%s.xml" % key)
                root = xml_document_parse(file_read(path), path)

            self.source_map[key] = Source(key, root)

            # Variables a document declares are loaded as the document is, so a document can refer to what an earlier one declared
            self._variable_load(xml_node_child(root, "variable-list"))

    ################################################################################################################################
    def _render_load(self, manifest, config):
        """Load every render type and the pages rendered to it."""

        for render_xml in xml_node_child_list(xml_node_child(manifest, "render-list", True), "render"):
            type = xml_node_attribute(render_xml, "type", True)

            if type not in (RENDER_HTML, RENDER_MARKDOWN):
                raise ToolError("render type '%s' is not valid" % type)

            if type in self.render_map:
                raise ToolError("render '%s' has already been defined" % type)

            render = Render(type)

            for out in xml_node_child_list(render_xml, "render-source"):
                key = xml_node_attribute(out, "key", True)
                source = xml_node_attribute(out, "source") or key

                if source in config.exclude or (len(config.include) > 0 and source not in config.include):
                    continue

                file = xml_node_attribute(out, "file")
                menu = xml_node_attribute(out, "menu")

                if menu is not None:
                    if type != RENDER_HTML:
                        raise ToolError("menu is only valid with the html render type")

                    render.menu = True

                render.order.append(key)
                render.out_map[key] = RenderOut(key, source, file, menu)

            self.render_map[type] = render

    ################################################################################################################################
    def _variable_load(self, variable_list):
        """Load the variables a document declares."""

        if variable_list is None:
            return

        for variable in xml_node_child_list(variable_list, "variable"):
            if not self.evaluate_if(variable):
                continue

            log(DEBUG, "    load variable %s = %s" % (xml_node_attribute(variable, "key", True), self.var_store.add_node(variable)))

    ################################################################################################################################
    def evaluate_if(self, node):
        """Does the condition on a node hold?

        A node with no condition always holds, which is what makes the condition worth writing only where it is needed."""

        expression = xml_node_attribute(node, "if")

        return True if expression is None else eval_expression(self.var_store.replace_str(expression))

    ################################################################################################################################
    def source_get(self, key):
        """A document by key."""

        if key not in self.source_map:
            raise ToolError("source '%s' does not exist" % key)

        return self.source_map[key]

    ################################################################################################################################
    def render_get(self, type):
        """A render type by name."""

        if type not in self.render_map:
            raise ToolError("render type '%s' does not exist" % type)

        return self.render_map[type]

    ################################################################################################################################
    def render_out_get(self, type, key):
        """A page by render type and key, or None when the page is not rendered in this build."""

        return self.render_map[type].out_map.get(key) if type in self.render_map else None

    ################################################################################################################################
    def link_verify(self):
        """Check that every link to a section of a page points at a section that page has.

        This can only be done once every page is rendered, since the sections a page has are only known once it has been. A link to
        a page that was not rendered in this build is skipped rather than reported, since there is nothing to check it against."""

        for link in self.link_list:
            anchor_map = self.page_anchor_map.get(link["page"])

            if anchor_map is None:
                continue

            if link["section"][1:] not in anchor_map:
                raise ToolError(
                    "page '%s' has a link to page '%s' section '%s' that does not exist"
                    % (link["source"], link["page"], link["section"])
                )

    ################################################################################################################################
    def _cache_key(self):
        """What the cache is keyed by, i.e. everything that changes what the commands would be."""

        # Written the compact way so the key is the same string it has always been, since the cache is keyed by it and committed
        return (
            "default" if len(self.key_var_map) == 0 else json.dumps(self.key_var_map, sort_keys=True, separators=(",", ":")),
            "all" if len(self.require_list) == 0 else "\n".join(self.require_list),
        )

    ################################################################################################################################
    def _cache_file(self):
        """The cache to read, which is the deploy copy when deploying or when there is no local copy to prefer."""

        file_local = os.path.join(self.path_doc, _FILE_CACHE_LOCAL)

        if self.deploy or not os.path.exists(file_local):
            return os.path.join(self.path_doc, _FILE_CACHE_DEPLOY)

        return file_local

    ################################################################################################################################
    def cache_read(self):
        """Load the cache, giving each document the commands that were run to build it."""

        file = self._cache_file()

        if not os.path.exists(file):
            return

        variable_key, require = self._cache_key()
        self.cache = json.loads(file_read(file))

        for key in sorted(self.source_map):
            cache_list = self.cache.get(variable_key, {}).get(require, {}).get(key)

            if cache_list is not None:
                self.source_map[key].cache_list = cache_list

                log(DETAIL, "cache load %s (key = %s, require = %s)" % (key, variable_key, require))

    ################################################################################################################################
    def cache_write(self):
        """Write back the cache, keeping what it holds for builds other than this one."""

        variable_key, require = self._cache_key()

        for key in sorted(self.source_map):
            cache_list = self.source_map[key].cache_list

            if cache_list is not None:
                self.cache = {} if self.cache is None else self.cache
                self.cache.setdefault(variable_key, {}).setdefault(require, {})[key] = cache_list

        if self.cache is not None:
            file = os.path.join(self.path_doc, _FILE_CACHE_DEPLOY if self.deploy else _FILE_CACHE_LOCAL)

            # Written the way it has always been written, down to the space before each colon, so that a cache regenerated by this
            # tool differs from the one in the repository only where the documentation itself changed
            file_write(file, json.dumps(self.cache, sort_keys=True, indent=3, separators=(",", " : ")) + "\n")

    ################################################################################################################################
    def cache_reset(self, key):
        """Throw away what the cache holds for a document so it is built again.

        A cache that no longer describes the document is not an error on its own, since the documentation changes and the commands
        change with it. It is an error when the caller said to use the cache and nothing else."""

        if self.cache_only:
            raise ToolError("cache reset disabled by --cache-only option")

        log(WARN, "cache will be reset for source %s and rendering retried automatically" % key)

        self.source_map[key].cache_list = None
