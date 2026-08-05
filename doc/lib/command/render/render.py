"""Document Renderer.

What every output type does the same way: decide which parts of a document apply at all, work out where each section sits and what
it depends on, and turn a run of text and markup into whatever the output type writes for it.

What an output type writes for a tag is a table rather than code, so most of the difference between html and markdown is data. The
rest is in the renderer for that type, which walks the sections and decides what a page looks like.

A document is filtered before it is rendered rather than while, so a renderer never sees a section that does not apply. Filtering is
also what builds the section map, since both walk the whole document."""

####################################################################################################################################
import re

from common.error import ToolError
from common.log import *
from common.xml import (
    xml_node_add,
    xml_node_attribute,
    xml_node_attribute_remove,
    xml_node_attribute_set,
    xml_node_child,
    xml_node_child_list,
    xml_node_content_add,
    xml_node_text,
)

# Anchor of a section, and the value that says the section does not take the anchor of the section that holds it
SECTION_ANCHOR = "anchor"
SECTION_ANCHOR_NO_INHERIT = "no-inherit"

# Tags whose content is text and markup rather than markup alone, so what they hold is rendered as text
_TAG_TEXT = ("admonition", "code-block", "list-item", "p", "summary", "title")

# Tags that are content however little they hold, so they are never read as a property of the node holding them
_TAG_CONTENT = _TAG_TEXT + ("table-cell", "table-column")

# A run of spaces, which is how the xml is laid out to be read rather than something to render
_SPACE_RUN_EXP = re.compile(" +")

# What each output type writes before and after a tag. A tag that is not listed for an output type cannot appear in a document
# rendered to it, which is what keeps html-only markup out of markdown.
_TAG_RENDER = {
    "markdown": {
        "admonition": ("\n> **", "\n"),
        "b": ("**", "**"),
        "backrest": ("{[project]}", ""),
        "br": ("\\n", ""),
        "cmd": ("`", "`"),
        "code": ("`", "`"),
        "exe": ("{[project-exe]}", ""),
        "file": ("`", "`"),
        "i": ("_", "_"),
        "id": ("`", "`"),
        "list-item": ("- ", "\n"),
        "param": ("`", "`"),
        "path": ("`", "`"),
        "pg-setting": ("`", "`"),
        "postgres": ("PostgreSQL", ""),
        "proper": ("", ""),
        "quote": ('"', '"'),
        "setting": ("`", "`"),
    },
    "text": {
        "admonition": ("", "\n\n"),
        "b": ("", ""),
        "backrest": ("{[project]}", ""),
        "br": ("\\n", ""),
        "br-option": ("", ""),
        "cmd": ("", ""),
        "code": ("", ""),
        "code-block": ("", ""),
        "exe": ("{[project-exe]}", ""),
        "file": ("", ""),
        "host": ("", ""),
        "i": ("", ""),
        "id": ("", ""),
        "list": ("", "\n"),
        "list-item": ("* ", "\n"),
        "p": ("", "\n\n"),
        "param": ("", ""),
        "path": ("", ""),
        "pg-setting": ("", ""),
        "postgres": ("PostgreSQL", ""),
        "proper": ("", ""),
        "quote": ('"', '"'),
        "setting": ("", ""),
    },
    "html": {
        "admonition": ('<div class="admonition">', "</div>"),
        "b": ("<b>", "</b>"),
        "backrest": ('<span class="backrest">{[project]}</span>', ""),
        "br": ("<br/>", ""),
        "br-option": ('<span class="br-option">', "</span>"),
        "br-setting": ('<span class="br-setting">', "</span>"),
        "cmd": ('<span class="cmd">', "</span>"),
        "code": ('<span class="id">', "</span>"),
        "code-block": ("<code-block>", "</code-block>"),
        "exe": ('<span class="file">{[project-exe]}</span>', ""),
        "file": ('<span class="file">', "</span>"),
        "host": ('<span class="host">', "</span>"),
        "i": ("<i>", "</i>"),
        "id": ('<span class="id">', "</span>"),
        "list": ('<ul class="list-unordered">', "</ul>"),
        "list-item": ('<li class="list-unordered">', "</li>"),
        "p": ("", ""),
        "param": ('<span class="br-option">', "</span>"),
        "path": ('<span class="path">', "</span>"),
        "pg-option": ('<span class="pg-option">', "</span>"),
        "pg-setting": ('<span class="pg-setting">', "</span>"),
        "postgres": ('<span class="postgres">PostgreSQL</span>', ""),
        "proper": ('<span class="host">', "</span>"),
        "quote": ("<q>", "</q>"),
        "setting": ('<span class="br-setting">', "</span>"),
        "user": ('<span class="user">', "</span>"),
    },
}


####################################################################################################################################
def child_list(node):
    """The children of a node that are part of the document rather than properties of it.

    A child that holds nothing but text and says nothing about itself is a property, e.g. the command an execute runs, and is read by
    name where it is needed rather than walked with the content. A tag that is text is content however little it holds."""

    return [
        child for child in node if child.tag != "text" and (child.tag in _TAG_CONTENT or len(child) > 0 or len(child.attrib) > 0)
    ]


####################################################################################################################################
def content_list(node):
    """The text and the markup a node holds, in the order they were written.

    A string is text and anything else is markup, which is all a caller walking mixed content needs to tell apart."""

    result = []

    if node.text:
        result.append(node.text)

    for child in node:
        result.append(child)

        if child.tail:
            result.append(child.tail)

    return result


####################################################################################################################################
class DocRender:
    """Renders a document to one output type."""

    def __init__(self, type, manifest=None, exe=False, key=None):
        self.type = type
        self.manifest = manifest
        self.exe = exe
        self.key = key  # Page being rendered, which is also the document unless the page says otherwise

        # A copy so that setting a tag applies to this renderer rather than to every renderer of the type
        self.tag_render = dict(_TAG_RENDER[type])

        self.root = None
        self.section_map = {}
        self.section_required = None
        self.pre_execute_map = {}
        self.toc = True

        if key is None:
            return

        out = manifest.render_out_get(type, key)
        self.root = manifest.source_get(key).root
        self.source = manifest.source_get(out.source)

        self._build(self.root)

        # Sections the caller asked for, which is how a partial build is limited to the part being worked on
        for path in manifest.require_list:
            if not path.startswith("/"):
                raise ToolError("path %s must begin with a /" % path)

            if path not in self.section_map:
                raise ToolError("required section '%s' does not exist" % path)

            self.section_required = {} if self.section_required is None else self.section_required
            self._require(path)

        # Register the anchors of this page so links to it from any page can be checked once every page is rendered
        manifest.page_anchor_map[key] = self._section_anchor_map()

        self.toc = xml_node_attribute(self.root, "toc") != "n"

    ################################################################################################################################
    def tag_set(self, tag, begin, end=""):
        """Set what this renderer writes before and after a tag."""

        self.tag_render[tag] = (begin, end)

    ################################################################################################################################
    def variable_replace(self, string):
        """Replace every variable in a string, which a renderer with no documentation to draw on cannot do."""

        return string if self.manifest is None or string is None else self.manifest.var_store.replace_str(string)

    ################################################################################################################################
    def pre_execute(self, host):
        """Commands to run while building the image for a host rather than against the host once it is up."""

        return self.pre_execute_map.get(host, [])

    ################################################################################################################################
    def _build(self, node, parent=None, path=None, path_prefix=None):
        """Filter a document down to what applies and record where each section sits."""

        name = node.tag

        # A node whose condition does not hold is not part of this build of the document
        if parent is not None and not self.manifest.evaluate_if(node):
            title = xml_node_child(node, "title")

            log(DEBUG, "            filtered %s%s" % (name, "" if title is None else ": " + self.process_text(title)))

            parent.remove(node)

            return

        if name == "section":
            path = self._build_section(node, parent, path)

            # A section that takes its content from another document renames the sections it takes, since where they are now is not
            # where they were written
            if xml_node_attribute(node, "source") is not None:
                self._build_source(node, path)
                path_prefix = path
        elif name == "link":
            # A link to a section of a document that has been taken into another one points at where the section is now
            if path_prefix is not None and xml_node_attribute(node, "section") is not None:
                xml_node_attribute_set(node, "section", path_prefix + xml_node_attribute(node, "section"))
        elif name == "execute":
            # A command marked pre is run while the image for the host is built, so it is skipped when the host is up
            if self.manifest.pre and xml_node_attribute(node, "pre") == "y":
                self.pre_execute_map.setdefault(self.variable_replace(xml_node_attribute(parent, "host")), []).append(node)
                xml_node_attribute_set(node, "skip", "y")

        # The text of a node is walked along with its content, since markup in a paragraph can be conditional too. A node that is
        # text itself holds both in the same place, so they are walked once.
        text = xml_node_text(node)
        walk = [] if text is None else [(text, child) for child in list(text)]

        if text is not node:
            walk += [(node, child) for child in child_list(node)]

        for child_parent, child in walk:
            self._build(child, child_parent, path, path_prefix)

            # A section that is worth logging makes the section holding it worth logging, so the hierarchy reads in full
            if child.tag == "section" and xml_node_attribute(child, "log") == "y":
                xml_node_attribute_set(node, "log", "y")

    ################################################################################################################################
    def _build_section(self, node, parent, path):
        """Record where a section sits and what it depends on, and return its path."""

        id = xml_node_attribute(node, "id", True)

        if path is not None:
            xml_node_attribute_set(node, "path-parent", path)

        path = "%s/%s" % ("" if path is None else path, id)

        self.section_map[path] = node
        xml_node_attribute_set(node, "path", path)

        # A section depends on the section before it unless it says otherwise, since the documentation is read in order
        depend = xml_node_attribute(node, "depend")
        depend_prev = None

        sibling_list = xml_node_child_list(parent, "section")
        index = sibling_list.index(node)

        if index > 0:
            depend_prev = xml_node_attribute(sibling_list[index - 1], "id")
        else:
            # The first section of a section depends on whatever the section holding it depends on, which for the first section of
            # a document is nothing
            depend_prev = xml_node_attribute(parent, "depend")

        if depend is not None:
            if depend == depend_prev and xml_node_attribute(node, "depend-default") is None:
                log(
                    WARN,
                    "section '%s' depend is set to '%s' which is the default, best to remove because it may become obsolete if a"
                    " new section is added in between" % (path, depend),
                )
        else:
            depend = depend_prev

        if depend is not None:
            # A depend that is not rooted is relative to the section holding this one
            if not depend.startswith("/"):
                parent_path = xml_node_attribute(parent, "path") if parent is not None else None
                depend = "/%s" % depend if parent_path is None else "%s/%s" % (parent_path, depend)

            if depend not in self.section_map:
                raise ToolError("section '%s' depend '%s' is not valid" % (id, depend))

            xml_node_attribute_set(node, "depend", depend)

        if depend_prev is not None:
            xml_node_attribute_set(node, "depend-default", depend_prev)

        # A section that runs commands is worth logging, which keeps the log to the sections that take a while
        log_section = self.exe and len(xml_node_child_list(node, "execute-list")) > 0

        xml_node_attribute_set(node, "log", "y" if log_section else "n")

        return path

    ################################################################################################################################
    def _build_source(self, node, path):
        """Take the content of another document into a section."""

        source = self.manifest.source_get(xml_node_attribute(node, "source")).root

        if xml_node_child(node, "title") is not None:
            raise ToolError("cannot specify title in section that sources another document")

        # The title comes from the document being taken rather than being said again here
        xml_node_content_add(xml_node_add(node, "title"), xml_node_attribute(source, "title", True))

        node.extend(xml_node_child_list(source, "section"))

        # Remove the source so the content is not taken again further down
        xml_node_attribute_remove(node, "source")

    ################################################################################################################################
    def _require(self, path, depend=True):
        """Add a section and everything it needs to the sections this build renders."""

        node = self.section_map[path]

        # Only a section that was asked for brings its own sections with it, since a section that is only a dependency is needed for
        # what it does rather than for what it says
        if depend:
            for child_path in sorted(self.section_map):
                if child_path == path or child_path.startswith("%s/" % path):
                    if child_path not in self.section_required:
                        log(INFO, "    " * (len(child_path.split("/")) - 2) + "        require section: %s" % child_path)

                        self.section_required[child_path] = True

        # A section that is depended on is required for what it does, and the section holding this one for what it does as well
        depend_path = xml_node_attribute(node, "depend")
        parent_path = xml_node_attribute(node, "path-parent")

        if depend_path is not None:
            self._require(depend_path, True)
        elif parent_path is not None:
            self._require(parent_path, False)

    ################################################################################################################################
    def is_required(self, section):
        """Are the commands in a section required by this build?"""

        return self.section_required is None or xml_node_attribute(section, "path") in self.section_required

    ################################################################################################################################
    def _section_anchor_map(self):
        """Every anchor this page has.

        An anchor is the id of a section prefixed with the anchors of the sections holding it, which is what the html renderer
        writes, unless a section says it does not take the anchor of the section holding it."""

        result = {}

        for path in self.section_map:
            id_list = []
            section = self.section_map[path]

            while section is not None:
                id_list.insert(0, xml_node_attribute(section, "id"))

                if xml_node_attribute(section, SECTION_ANCHOR) == SECTION_ANCHOR_NO_INHERIT:
                    break

                parent_path = xml_node_attribute(section, "path-parent")
                section = self.section_map.get(parent_path) if parent_path is not None else None

            result["/".join(id_list)] = True

        return result

    ################################################################################################################################
    def _process_link(self, node):
        """Render a link, which is where a document says what it points at rather than how to get there."""

        url = xml_node_attribute(node, "url")

        # What the link says is text and markup, since a link to an option names it the way the rest of the documentation does
        value = self.process_text(node)

        if url is None:
            page = self.variable_replace(xml_node_attribute(node, "page"))
            section = xml_node_attribute(node, "section")

            # A section link is written rooted so that stripping the leading / gives the anchor
            if section is not None and not section.startswith("/"):
                raise ToolError("link section '%s' must begin with '/'" % section)

            # A link to a section of the page it is on is a link to the section
            if page is not None and section is not None and page == self.key:
                page = None

            if page is not None:
                # Record the link so it can be checked once every page is rendered and the anchors of each are known
                if section is not None:
                    self.manifest.link_list.append({"source": self.key, "page": page, "section": section})

                anchor = "" if section is None else "#" + section[1:]

                # A page that is not part of this build is on the website rather than beside this one
                if self.manifest.render_out_get(self.type, page) is None:
                    url = "{[backrest-url-base]}/%s.html%s" % (page, anchor)
                elif self.type == "html":
                    url = "%s.html%s" % (page, anchor)
                elif self.type == "markdown":
                    # Markdown has no anchor for a section of another page, so the website is where that link can point
                    url = "{[backrest-url-base]}/%s.html%s" % (page, anchor) if section is not None else "%s.md" % page
                else:
                    raise ToolError("page links not supported for type %s, value '%s'" % (self.type, value))
            else:
                section = xml_node_attribute(node, "section", True)

                if section not in self.section_map:
                    raise ToolError("section link '%s' does not exist" % section)

                if self.type == "html":
                    url = "#%s" % section[1:]
                else:
                    # Markdown builds the anchor from the title of the section rather than from its id
                    title = self.process_text(xml_node_child(self.section_map[section], "title", True)).lower()

                    url = "#%s" % "".join(char for char in title if char.isalnum() or char in "- _").replace(" ", "-")

        if self.type == "html":
            return '<a href="%s">%s</a>' % (url, value)

        if self.type == "markdown":
            return "[%s](%s)" % (value, url)

        return value

    ################################################################################################################################
    def _process_admonition(self, node):
        """Render what an admonition puts before what it says, i.e. the kind of admonition it is."""

        type = xml_node_attribute(node, "type", True)

        if self.type == "html":
            return '<div class="%s">%s:</div><div class="%s-text">' % (type, type.upper(), type)

        return "%s: " % type.upper()

    ################################################################################################################################
    def process_tag(self, node):
        """Render a tag and everything it holds."""

        tag = node.tag

        if tag == "link":
            return self._process_link(node)

        if tag not in self.tag_render:
            raise ToolError("invalid type %s or tag %s" % (self.type, tag))

        begin, end = self.tag_render[tag]
        result = begin

        # An admonition in the help is markup of a paragraph rather than a paragraph of its own, so it says what kind it is here
        if tag == "admonition":
            result += self._process_admonition(node)

        if tag in _TAG_TEXT:
            result += self.process_text(node)
        elif node.text is not None and node.text.strip() != "":
            result += node.text
        else:
            for child in node:
                result += self.process_tag(child)

        if tag == "admonition" and self.type == "html":
            result += "</div>"

        return result + end

    ################################################################################################################################
    def process_text(self, node):
        """Render the text and markup a node holds."""

        result = ""
        last_tag = "body"

        for item in content_list(node):
            if isinstance(item, str):
                if '"' in item:
                    raise ToolError("unable to process quotes in string (use <quote> instead):\n%s" % item)

                # Text that runs over more than one line is how the xml is laid out to be read rather than something to render
                if "\n" not in item:
                    result += item
            else:
                # Separate paragraphs, which is needed because the help is rendered to html and then put into a document rather than
                # being rendered as part of one. The linefeed makes the output easier to diff.
                if last_tag == "p" and self.type == "html":
                    result += "<br/>\n"

                    if item.tag == "p":
                        result += "<br/>\n"

                result += self.process_tag(item)
                last_tag = item.tag

        # Runs of spaces and a space starting a line are how the xml is laid out to be read rather than something to render
        result = _SPACE_RUN_EXP.sub(" ", result)
        result = "\n".join(line[1:] if line.startswith(" ") else line for line in result.split("\n"))

        if self.type == "html":
            result = result.replace("master", "ma&#115;ter")

        if self.type == "text":
            result = result.replace("&mdash;", "--").replace("&lt;", "<").replace("&ge;", ">=")

        return self.variable_replace(result)
