"""Html Renderer.

Renders a document as a page. The page is built as a tree of elements and written once at the end, so the code that decides what a
page holds never deals with markup, and where a linefeed goes is decided in one place.

Where those linefeeds go matters more than it looks. The pages are compared against the pages of the previous build to check that a
change to the tool changed nothing about the documentation, and output that is all on one line makes that comparison useless."""

####################################################################################################################################
import os
import re
import shutil
import struct

from common.error import ToolError
from common.log import *
from common.storage import file_read, file_write, path_create, path_list
from common.xml import xml_node_attribute, xml_node_child, xml_node_child_list, xml_node_content, xml_node_field, xml_node_text
from command.render.execute import CONFIG_MARK_ADD, CONFIG_MARK_REMOVE, CONFIG_MARK_SAME, CacheInvalidError, DocExecute
from command.render.manifest import RENDER_HTML
from command.render.render import SECTION_ANCHOR, SECTION_ANCHOR_NO_INHERIT, child_list

# How deep a page may be sectioned before the headings run out of styles
_SECTION_DEPTH_MAX = 3

# What a page says about itself that is the same on every page
_DOCTYPE = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">'
_ANALYTICS = (
    '<script async src="https://www.googletagmanager.com/gtag/js?id=G-VKCRNV73H1"></script>',
    "<script>window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}"
    "gtag('js',new Date());gtag('config','G-VKCRNV73H1');</script>",
)

# What a block that scrolls sideways says so a reader who cannot use a mouse can still reach what has scrolled out of it
_SCROLL = 'tabindex="0"'

# What a style or a script says about itself, which the page carries for a browser rather than for a reader
_COMMENT_EXP = re.compile(r"/\*.*?\*/", re.DOTALL)

# Leading linefeeds and trailing whitespace of a code block, which are how it sits in the xml rather than part of it
_BLOCK_BEGIN_EXP = re.compile(r"^\n+")
_BLOCK_END_EXP = re.compile(r"\s+$")

# What a line of a configuration file is called, which is what decides how it looks and what marks it
_CONFIG_CLASS_MAP = {
    CONFIG_MARK_ADD: "config-line-add",
    CONFIG_MARK_REMOVE: "config-line-remove",
    CONFIG_MARK_SAME: "config-line",
}


####################################################################################################################################
class HtmlElement:
    """One element of a page."""

    def __init__(self, type, class_name=None, content=None, id=None, ref=None, extra=None, pre=False):
        self.type = type
        self.class_name = class_name
        self.content = content
        self.id = id
        self.ref = ref
        self.extra = extra
        self.pre = pre  # Is the content laid out already, so nothing here may lay it out again?
        self.child_list = []

    ################################################################################################################################
    def add(self, element):
        """Add an element."""

        self.child_list.append(element)

        return element

    ################################################################################################################################
    def add_new(self, type, class_name=None, **kwargs):
        """Add a new element."""

        return self.add(HtmlElement(type, class_name, **kwargs))


####################################################################################################################################
class HtmlBuilder:
    """Builds a page from the elements it holds."""

    def __init__(self, name, title, favicon, card, card_size, description):
        self.name = name
        self.title = title
        self.favicon = favicon
        self.card = card
        self.card_size = card_size
        self.description = description

        self.body = HtmlElement("body")
        self._pre_prior = False

    ################################################################################################################################
    @staticmethod
    def _escape(string):
        """Escape what cannot appear in an attribute."""

        return string.replace("&", "&amp;").replace("<", "&lt;")

    ################################################################################################################################
    def _element_render(self, element):
        """Render an element and everything it holds."""

        result = ""

        # Put a linefeed before laid out content unless the element before it was also laid out, which makes the page diffable
        if element.type == "pre":
            if not self._pre_prior:
                result += "\n"

            self._pre_prior = True
        else:
            self._pre_prior = False

        result += "<%s%s%s%s%s>" % (
            element.type,
            "" if element.class_name is None else ' class="%s"' % element.class_name,
            "" if element.ref is None else ' href="%s"' % element.ref,
            "" if element.id is None else ' id="%s"' % element.id,
            "" if element.extra is None else " %s" % element.extra,
        )

        if element.content is not None:
            if element.pre:
                result += element.content.replace("&", "&amp;")
            else:
                # Linefeeds around the content make the page diffable
                result += "\n%s\n" % element.content.strip()
        else:
            for child in element.child_list:
                result += self._element_render(child)

        result += "</%s>" % element.type

        return result + ("\n" if element.type == "pre" else "")

    ################################################################################################################################
    def render(self, analytics=False):
        """Render the page."""

        result = _DOCTYPE
        result += '<html xmlns="http://www.w3.org/1999/xhtml">'
        result += "<head>"
        result += "\n<title>" + self._escape(self.title) + "\n"
        result += "</title>"
        result += '<meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>\n'

        # Lay the page out at the width of the device rather than at the width of a desktop scaled down to fit
        result += '<meta name="viewport" content="width=device-width, initial-scale=1"></meta>\n'

        result += '<meta property="og:site_name" content="%s"></meta>\n' % self._escape(self.name)
        result += '<meta property="og:title" content="%s"></meta>\n' % self._escape(self.title)
        result += '<meta property="og:type" content="website"></meta>\n'

        if self.favicon is not None:
            result += '<link rel="icon" href="%s" type="image/svg+xml"></link>\n' % self.favicon

        # The card is what a link to the page looks like where it is posted. It is laid out at 1200x630 because that is the frame
        # the sites that show a preview crop to, and a card that is not that shape loses its top and bottom to the crop.
        if self.card is not None:
            result += '<meta property="og:image" content="{[backrest-url-base]}/%s"></meta>\n' % self.card
            result += '<meta property="og:image:type" content="image/png"></meta>\n'
            result += '<meta property="og:image:width" content="%d"></meta>\n' % self.card_size[0]
            result += '<meta property="og:image:height" content="%d"></meta>\n' % self.card_size[1]
            result += '<meta property="og:image:alt" content="%s"></meta>\n' % self._escape(self.name)

            # Without this the preview is a thumbnail beside the text rather than the card above it
            result += '<meta name="twitter:card" content="summary_large_image"></meta>\n'

        if self.description is not None:
            result += '<meta name="description" content="%s"></meta>\n' % self._escape(self.description)
            result += '<meta property="og:description" content="%s"></meta>\n' % self._escape(self.description)

        result += '<link rel="stylesheet" href="default.css" type="text/css"></link>\n'

        # Deferred because the script reads the page it marks, and nothing on the page waits for it
        result += '<script src="default.js" defer="defer"></script>\n'

        if analytics:
            for line in _ANALYTICS:
                result += line + "\n"

        result += "</head>"
        result += self._element_render(self.body)

        return result + "</html>"


####################################################################################################################################
class DocHtmlPage(DocExecute):
    """Renders one document as a page."""

    def __init__(self, manifest, key, menu, exe):
        super().__init__(RENDER_HTML, manifest, key, exe)

        self.menu = menu

    ################################################################################################################################
    def process(self):
        """Render the page."""

        var_store = self.manifest.var_store
        render = self.manifest.render_get(RENDER_HTML)

        title = xml_node_attribute(self.root, "title", True)
        subtitle = xml_node_attribute(self.root, "subtitle")

        tagline = var_store.get("project-tagline")

        card = None if var_store.test("card", "n") else var_store.get("project-card")

        builder = HtmlBuilder(
            var_store.replace_str("{[project]}" + ("" if tagline is None else " - " + tagline)),
            var_store.replace_str(title + ("" if subtitle is None else " - " + subtitle)),
            var_store.get("project-favicon"),
            card,
            None if card is None else _png_size(os.path.join(self.manifest.path_doc, "resource", card)),
            var_store.replace_str(xml_node_field(self.root, "description", True).strip()),
        )

        header = builder.body.add_new("div", "page-header")
        header.add_new("div", "page-header-title", content=title)

        if subtitle is not None:
            header.add_new("div", "page-header-subtitle", content=subtitle)

        if self.menu:
            menu_body = builder.body.add_new("div", "page-menu").add_new("div", "menu-body")

            # The menu is in the order the manifest lists the pages rather than in the order they were rendered
            for key in render.order:
                # The page a reader is on is not in the menu, since they are already there
                if key == self.key:
                    continue

                out = self.manifest.render_out_get(RENDER_HTML, key)

                if out is not None and out.menu is not None:
                    menu_body.add_new("div", "menu").add_new(
                        "a",
                        "menu-link",
                        content=out.menu,
                        ref="{[project-url-root]}" if key == "index" else "%s.html" % key,
                    )

        toc_body = None

        if self.toc:
            # A page with contents puts them in a column beside the text, which the body says so the style does not have to work
            # it out from what the page happens to hold
            builder.body.class_name = "page-sidebar"

            # The contents are held twice over: the outer element is as tall as the text beside it, and the inner one is what
            # sticks, so it can never be carried past the text into the footer below
            toc = builder.body.add_new("div", "page-toc").add_new("div", "page-toc-inner")
            toc.add_new("div", "page-toc-header").add_new("div", "page-toc-title", content="Table of Contents")
            toc_body = toc.add_new("div", "page-toc-body")

        body = builder.body.add_new("div", "page-body")

        # A section set aside goes first whatever the document says, since the text can only run beside what is already there. Which
        # sections were written after it is kept as a class, so a page too narrow to put it down the side can put it back where the
        # document had it rather than at the end. The document keeps its own order for every other way it is rendered.
        aside_list = []
        before_list = []
        after_list = []

        for section in xml_node_child_list(self.root, "section"):
            if xml_node_attribute(section, "html") == "n":
                continue

            if xml_node_attribute(section, "aside") == "y":
                aside_list.append(section)
            elif len(aside_list) > 0:
                after_list.append(section)
            else:
                before_list.append(section)

        for section_no, section in enumerate(aside_list + before_list + after_list):
            element, toc_element = self._section_process(section, None, str(section_no + 1), 1)

            if section in after_list:
                element.class_name += " section-after-aside"

            body.add(element)

            if toc_body is not None and toc_element is not None:
                toc_body.add(toc_element)

        builder.body.add_new("div", "page-footer", content="{[html-footer]}")

        return builder.render(analytics=var_store.test("analytics", "y"))

    ################################################################################################################################
    def _section_process(self, section, anchor, section_no, depth):
        """Render a section and everything it holds."""

        if xml_node_attribute(section, "log") == "y":
            log(INFO, "    " * (depth + 1) + "process section: %s" % xml_node_attribute(section, "path"))

        if depth > _SECTION_DEPTH_MAX:
            raise ToolError("section depth of %d exceeds maximum" % depth)

        # A section takes the anchor of the section holding it unless it says otherwise, so a link to it reads as a path
        anchor = (
            ""
            if xml_node_attribute(section, SECTION_ANCHOR) == SECTION_ANCHOR_NO_INHERIT
            else ("" if anchor is None else anchor + "/")
        ) + xml_node_attribute(section, "id", True)

        toc_element = HtmlElement("div", "section%d-toc" % depth)

        # A section set aside is put down the side of the page rather than in the run of it, which the style does from the class
        element = HtmlElement(
            "div", "section%d%s" % (depth, " section-aside" if xml_node_attribute(section, "aside") == "y" else "")
        )

        element.add_new("a", id=anchor)

        title = self.process_text(xml_node_child(section, "title", True))

        # A section can leave its header off the page when the page already says what the header would say. The section keeps its
        # title for the contents and its anchor for a link to point at.
        if xml_node_attribute(section, "header") != "n":
            element.add_new("div", "section%d-header" % depth).add_new("div", "section%d-title" % depth, content=title)

        # The contents are not numbered. The numbering says where a section sits on the page, which the contents already show by
        # the order and the indent of their entries, so beside the text it would only take room from the titles.

        # A section can give the contents a shorter title than it gives the page, since a heading has the width of the text to say
        # what a section is and the contents beside the text have a column
        title_toc = xml_node_attribute(section, "toc-title")

        toc_element.add_new("div", "section%d-toc-title" % depth).add_new(
            "a", content=title if title_toc is None else title_toc, ref="#%s" % anchor
        )

        text = xml_node_text(section)

        if text is not None:
            element.add_new("div", "section-intro", content=self.process_text(text))

        body = element.add_new("div", "section-body")
        child_no = 1

        for child in child_list(section):
            log(DEBUG, "    " * (depth + 2) + "process child %s" % child.tag)

            # Something the document keeps for another way of rendering it and this one leaves out. A condition cannot say this
            # because a condition is evaluated once for the document rather than once for each way it is rendered.
            if xml_node_attribute(child, "html") == "n":
                continue

            if child.tag == "execute-list":
                self._execute_list_process(section, child, body, depth)
            elif child.tag == "code-block":
                body.add_new("pre", "code-block", content=_code_block(child), pre=True, extra=_SCROLL)
            elif child.tag == "table":
                self._table_process(child, body)
            elif child.tag == "p":
                body.add_new("div", "section-body-text", content=self.process_text(child))
            elif child.tag == "backrest-config":
                config = self._config_process(section, child, depth + 3, backrest=True)

                if config is not None:
                    body.add(config)
            elif child.tag == "postgres-config":
                config = self._config_process(section, child, depth + 3, backrest=False)

                if config is not None:
                    body.add(config)
            elif child.tag == "list":
                list_element = body.add_new("ul", "list-unordered")

                for item in child_list(child):
                    list_element.add_new("li", "list-unordered", content=self.process_text(xml_node_text(item)))
            elif child.tag == "sponsor-list":
                self._sponsor_process(child, body)
            elif child.tag in ("subtitle", "subsubtitle"):
                body.add_new("div", "section%d-%s" % (depth, child.tag), content=self.process_text(xml_node_text(child)))
            elif child.tag == "section":
                child_element, child_toc = self._section_process(child, anchor, "%s.%d" % (section_no, child_no), depth + 1)

                body.add(child_element)

                if child_toc is not None:
                    toc_element.add(child_toc)

                child_no += 1
            elif child.tag == "admonition":
                type = xml_node_attribute(child, "type", True)
                admonition = body.add_new("div", "admonition")

                admonition.add_new("div", type, content="%s: " % type.upper())
                admonition.add_new("div", "%s-text" % type, content=self.process_text(child))
            else:
                self.section_child_process(section, child, depth + 1)

        return element, None if xml_node_attribute(section, "toc") == "n" else toc_element

    ################################################################################################################################
    def _execute_list_process(self, section, node, body, depth):
        """Render the commands of a section and what they wrote."""

        show = xml_node_attribute(node, "show") != "n"
        host_name = self.manifest.var_store.replace_str(xml_node_attribute(node, "host", True))
        execute_body = None

        if show:
            execute = body.add_new("div", "execute")
            execute.add_new(
                "div",
                "execute-title",
                content='<span class="host">%s</span> <b>&#x21d2;</b> %s'
                % (host_name, self.process_text(xml_node_child(node, "title", True))),
            )
            execute_body = execute.add_new("div", "execute-body")

        for command in xml_node_child_list(node, "execute"):
            show_command = xml_node_attribute(command, "show") != "n"
            expect_error = xml_node_attribute(command, "err-expect") is not None

            cmd, output = self.execute(section, host_name, command, indent=depth + 3, show=show and show_command)

            if not (show and show_command):
                continue

            # The command runs beside the button that copies it rather than under it, so the button covers nothing a reader is
            # reading however wide the command is
            command_element = execute_body.add_new("div", "execute-body-cmd")

            command_element.add_new("pre", "execute-cmd", content=cmd.replace("\n", "\n   "), pre=True, extra=_SCROLL)
            _copy_add(command_element, "command")

            highlight = self.manifest.var_store.replace_str(xml_node_field(command, "exe-highlight"))
            found = False

            if output is not None:
                if xml_node_field(command, "exe-highlight-type") == "error":
                    expect_error = True

                # What a command wrote is one block that scrolls, with the lines held in a block of their own inside it, so that a
                # line that is marked is marked across the width of the widest line rather than across the width of the part of it
                # that is in view, and so that scrolling to the end of a line does not leave the lines around it behind
                line_list = execute_body.add_new("div", "execute-body-output", extra=_SCROLL).add_new("div", "execute-line-list")

                # Runs of lines are grouped by whether they are highlighted, so a run is one element rather than one per line
                previous = None
                run = None

                for line in output.split("\n"):
                    highlighted = highlight is not None and re.search(highlight, line) is not None

                    if previous is not None and highlighted != previous:
                        line_list.add_new("pre", _output_class(previous, expect_error), content=run, pre=True)
                        run = None

                    run = line if run is None else run + "\n" + line
                    previous = highlighted
                    found = found or highlighted

                # Whatever is left is the last run, since output always holds at least one line
                line_list.add_new("pre", _output_class(previous, expect_error), content=run, pre=True)

            if self.exe and self.is_required(section) and highlight is not None and not found:
                raise ToolError("unable to find a match for highlight: %s" % highlight)

    ################################################################################################################################
    def _table_process(self, node, body):
        """Render a table."""

        title = xml_node_child(node, "title")
        table = body.add_new("table", "table")
        column_list = []

        if title is not None:
            label = xml_node_attribute(title, "label")
            text = self.process_text(title)

            table.add_new("caption", "table-caption", content=text if label is None else "%s: %s" % (label, text))

        header = xml_node_child(node, "table-header")

        if header is not None:
            column_list = xml_node_child_list(header, "table-column")
            row = table.add_new("tr", "table-header-row")

            for column in column_list:
                align = xml_node_attribute(column, "align") or "left"
                fill = " table-header-fill" if xml_node_attribute(column, "fill") == "y" else ""

                row.add_new("th", "table-header-%s%s" % (align, fill), content=self.process_text(column))

        for data_row in xml_node_child_list(xml_node_child(node, "table-data", True), "table-row"):
            row = table.add_new("tr", "table-row")

            for cell_idx, cell in enumerate(xml_node_child_list(data_row, "table-cell")):
                align = xml_node_attribute(column_list[cell_idx], "align") or "left" if len(column_list) > 0 else "left"

                row.add_new("td", "table-data-%s" % align, content=self.process_text(cell))

    ################################################################################################################################
    def _sponsor_process(self, node, body):
        """Render the sponsors, each as a link with a logo for a light and a dark page."""

        sponsor_list = body.add_new("div", "sponsor-list")

        for sponsor in child_list(node):
            link = sponsor_list.add_new("div", "sponsor").add_new("a", ref=xml_node_attribute(sponsor, "url", True))
            width = xml_node_attribute(sponsor, "width", True)
            image = xml_node_attribute(sponsor, "img", True)
            image_dark = xml_node_attribute(sponsor, "img-dark") or image
            name = xml_node_content(sponsor)

            link.add_new(
                "img",
                "sponsor-img sponsor-img-light",
                extra='src="sponsor/%s" alt="%s" width="%s"' % (image, name, width),
            )
            link.add_new(
                "img",
                "sponsor-img sponsor-img-dark",
                extra='src="sponsor/%s" alt="%s" width="%s"' % (image_dark, name, width),
            )

    ################################################################################################################################
    def _config_process(self, section, node, depth, backrest):
        """Render a configuration change as the file it leaves behind."""

        if backrest:
            file, config, show = self.backrest_config(section, node, depth)
        else:
            file, config, show = self.postgres_config(section, node, depth)

        if not show:
            return None

        host_name = self.manifest.var_store.replace_str(xml_node_attribute(node, "host", True))
        element = HtmlElement("div", "config")

        # The title carries the button that copies the file, since what is shown of the file is a change to it rather than the file
        # itself. The button comes before what the title says so that the title wraps around it rather than under it.
        title = element.add_new("div", "config-title")

        _copy_add(title, "configuration")

        title.add_new(
            "span",
            "config-title-text",
            content='<span class="host">%s</span>:<span class="file">%s</span> <b>&#x21d2;</b> %s'
            % (host_name, file, self.process_text(xml_node_child(node, "title", True))),
        )

        # The lines are held in a block of their own inside the block that scrolls, so that a line the change marks is marked across
        # the width of the widest line rather than across the width of the part of it that is in view
        output = element.add_new("div", "config-body").add_new("div", "config-body-output", extra=_SCROLL)
        line_list = output.add_new("div", "config-line-list")

        for mark, line in config:
            line_list.add_new("pre", _CONFIG_CLASS_MAP[mark], content=line, pre=True)

        return element


####################################################################################################################################
def _png_size(file):
    """Read what a png says it measures.

    The card says its size in the page so a crawler can lay the preview out before it has the image, and the size is read from the
    image rather than written down so the two cannot disagree."""

    with open(file, "rb") as handle:
        return struct.unpack(">II", handle.read(24)[16:24])


####################################################################################################################################
def _resource_render(text, line_comment=False):
    """A style or a script with what it says about itself taken out.

    Where the lines fall is left alone, so one build of the documentation can still be compared with the next, which is the same
    reason the pages themselves are not written on one line."""

    result = []

    for line in _COMMENT_EXP.sub("", text).split("\n"):
        line = line.rstrip()

        # A comment on a line of its own goes with the line. One after code is left, since telling code from a string that holds
        # what looks like a comment takes a parser rather than a rule, and the scripts here do not write one.
        if line_comment and line.lstrip().startswith("//"):
            continue

        # A line that held nothing but a comment is empty now, and a run of empty lines is left as one
        if line == "" and (len(result) == 0 or result[-1] == ""):
            continue

        result.append(line)

    return "\n".join(result).strip("\n") + "\n"


####################################################################################################################################
def _copy_add(element, what):
    """Add the button that hands a reader what a block is showing rather than what the documentation wrote around it.

    What the button is drawn with is left to the style, which says it once for the documentation rather than once for every block
    that carries one."""

    element.add_new("button", "code-copy", extra='type="button" title="Copy the %s" aria-label="Copy the %s"' % (what, what))


####################################################################################################################################
def _output_class(highlighted, expect_error):
    """What a run of output is called, which is what decides how it looks."""

    if not highlighted:
        return "execute-line"

    return "execute-line-highlight" + ("-error" if expect_error else "")


####################################################################################################################################
def _code_block(node):
    """The content of a code block with the indent it was written at in the xml taken off."""

    value = _BLOCK_END_EXP.sub("", _BLOCK_BEGIN_EXP.sub("", xml_node_content(node)))

    # The line with the least indent decides how much indent to take off every line, so what is left is what was written. A blank
    # line has no indent, so a block with one in it is left as it is.
    indent = min(len(line.rstrip()) - len(line.strip()) for line in value.split("\n"))

    return "\n".join(line[indent:] if line.startswith(" " * indent) else line for line in value.split("\n"))


####################################################################################################################################
def html_render(manifest, path_doc, path_out, exe):
    """Render every page and everything that goes beside them."""

    render = manifest.render_get(RENDER_HTML)
    var_store = manifest.var_store

    style = file_read(os.path.join(path_doc, "resource/html/default.css"))
    script = file_read(os.path.join(path_doc, "resource/html/default.js"))

    file_write(os.path.join(path_out, "default.css"), _resource_render(style))
    file_write(os.path.join(path_out, "default.js"), _resource_render(script, line_comment=True))
    shutil.copyfile(os.path.join(path_doc, "resource/slogo.svg"), os.path.join(path_out, "slogo.svg"))

    favicon = var_store.get("project-favicon")

    if favicon is not None:
        shutil.copyfile(os.path.join(path_doc, "resource", favicon), os.path.join(path_out, favicon))

    card = var_store.get("project-card")

    if card is not None and not var_store.test("card", "n"):
        shutil.copyfile(os.path.join(path_doc, "resource", card), os.path.join(path_out, card))

    if not var_store.test("sponsor", "n"):
        path_sponsor = os.path.join(path_doc, "resource/sponsor")
        path_sponsor_out = os.path.join(path_out, "sponsor")

        path_create(path_sponsor_out)

        for name in path_list(path_sponsor):
            shutil.copyfile(os.path.join(path_sponsor, name), os.path.join(path_sponsor_out, name))

    for key in sorted(render.out_map):
        log(INFO, "    render out: %s" % key)

        out = render.out_map[key]

        def build():
            return var_store.replace_str(DocHtmlPage(manifest, key, render.menu, exe).process())

        try:
            html = build()
        except CacheInvalidError:
            # The cache no longer describes the document, so throw it away and build the document for real
            manifest.cache_reset(out.source)
            html = build()

        file_write(os.path.join(path_out, out.file if out.file is not None else "%s.html" % key), html)
