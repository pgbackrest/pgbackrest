"""Html Renderer.

Renders a document as a page. The page is built as a tree of elements and written once at the end, so the code that decides what a
page holds never deals with markup, and where a linefeed goes is decided in one place.

Where those linefeeds go matters more than it looks. The pages are compared against the pages of the previous build to check that a
change to the tool changed nothing about the documentation, and output that is all on one line makes that comparison useless."""

####################################################################################################################################
import os
import re
import shutil

from common.error import ToolError
from common.log import *
from common.storage import file_read, file_write, path_create, path_list
from common.xml import xml_node_attribute, xml_node_child, xml_node_child_list, xml_node_content, xml_node_field, xml_node_text
from command.render.execute import CacheInvalidError, DocExecute
from command.render.manifest import RENDER_HTML
from command.render.render import SECTION_ANCHOR, SECTION_ANCHOR_NO_INHERIT, child_list

# How deep a page may be sectioned before the numbering runs out of styles
_SECTION_DEPTH_MAX = 3

# What a page says about itself that is the same on every page
_DOCTYPE = '<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">'
_ANALYTICS = (
    '<script async src="https://www.googletagmanager.com/gtag/js?id=G-VKCRNV73H1"></script>',
    "<script>window.dataLayer=window.dataLayer||[];function gtag(){dataLayer.push(arguments);}"
    "gtag('js',new Date());gtag('config','G-VKCRNV73H1');</script>",
)

# A comment in a style, which a page that is not being written to be read does not carry
_COMMENT_EXP = re.compile(r"/\*.*?\*/")

# Leading linefeeds and trailing whitespace of a code block, which are how it sits in the xml rather than part of it
_BLOCK_BEGIN_EXP = re.compile(r"^\n+")
_BLOCK_END_EXP = re.compile(r"\s+$")


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

    def __init__(self, name, title, favicon, logo, description, pretty, compact, css):
        self.name = name
        self.title = title
        self.favicon = favicon
        self.logo = logo
        self.description = description
        self.pretty = pretty
        self.compact = compact
        self.css = css

        self.body = HtmlElement("body")
        self._pre_prior = False

    ################################################################################################################################
    def _indent(self, depth):
        """Indent to a depth, which a page that is not being written to be read does not do."""

        return "  " * depth if self.pretty else ""

    ################################################################################################################################
    def _lf(self):
        """End a line, which a page that is not being written to be read does not do."""

        return "\n" if self.pretty else ""

    ################################################################################################################################
    @staticmethod
    def _escape(string):
        """Escape what cannot appear in an attribute."""

        return string.replace("&", "&amp;").replace("<", "&lt;")

    ################################################################################################################################
    def _element_render(self, element, depth):
        """Render an element and everything it holds."""

        result = ""

        # Put a linefeed before laid out content unless the element before it was also laid out, which makes the page diffable
        if element.type == "pre" and not self.pretty:
            if not self._pre_prior:
                result += "\n"

            self._pre_prior = True
        else:
            self._pre_prior = False

        result += "%s<%s%s%s%s%s>" % (
            self._indent(depth),
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
                result += "\n%s\n%s" % (element.content.strip(), self._indent(depth))
        else:
            # An anchor with nothing in it marks a place on the page rather than holding anything, so it stays on one line
            anchor_empty = element.type == "a" and len(element.child_list) == 0

            if not anchor_empty:
                result += self._lf()

            for child in element.child_list:
                result += self._element_render(child, depth + 1)

            if not anchor_empty:
                result += self._indent(depth)

        result += "</%s>" % element.type

        return result + ("\n" if element.type == "pre" else self._lf())

    ################################################################################################################################
    def render(self, analytics=False):
        """Render the page."""

        result = self._indent(0) + _DOCTYPE + self._lf()
        result += self._indent(0) + '<html xmlns="http://www.w3.org/1999/xhtml">' + self._lf()
        result += self._indent(0) + "<head>" + self._lf()
        result += self._indent(1) + "\n<title>" + self._indent(2) + self._escape(self.title) + "\n"
        result += self._indent(1) + "</title>" + self._lf()
        result += self._indent(1) + '<meta http-equiv="Content-Type" content="text/html;charset=utf-8"></meta>\n'

        if not self.compact:
            result += '%s<meta property="og:site_name" content="%s"></meta>\n' % (self._indent(1), self._escape(self.name))
            result += '%s<meta property="og:title" content="%s"></meta>\n' % (self._indent(1), self._escape(self.title))
            result += '%s<meta property="og:type" content="website"></meta>\n' % self._indent(1)

            if self.favicon is not None:
                result += '%s<link rel="icon" href="%s" type="image/svg+xml"></link>\n' % (self._indent(1), self.favicon)

            if self.logo is not None:
                result += '%s<meta property="og:image:type" content="image/png"></meta>\n' % self._indent(1)
                result += '%s<meta property="og:image" content="{[backrest-url-base]}/%s"></meta>\n' % (
                    self._indent(1),
                    self.logo,
                )

            if self.description is not None:
                result += '%s<meta name="description" content="%s"></meta>\n' % (
                    self._indent(1),
                    self._escape(self.description),
                )
                result += '%s<meta property="og:description" content="%s"></meta>\n' % (
                    self._indent(1),
                    self._escape(self.description),
                )

        if self.css is not None:
            css = self.css

            # A page that is not being written to be read carries the style with the whitespace and comments taken out
            if not self.pretty:
                css = "\n".join(line.lstrip() for line in css.split("\n")).replace("\n", "")
                css = _COMMENT_EXP.sub("", css)

            result += '%s<style type="text/css">\n%s\n%s</style>\n' % (self._indent(1), css.strip(), self._indent(1))
        else:
            result += '%s<link rel="stylesheet" href="default.css" type="text/css"></link>\n' % self._indent(1)

        if analytics:
            for line in _ANALYTICS:
                result += self._indent(1) + line + "\n"

        result += self._indent(0) + "</head>" + self._lf()
        result += self._element_render(self.body, 0)

        return result + self._indent(0) + "</html>" + self._lf()


####################################################################################################################################
class DocHtmlPage(DocExecute):
    """Renders one document as a page."""

    def __init__(self, manifest, key, menu, exe, compact, css, pretty):
        super().__init__(RENDER_HTML, manifest, key, exe)

        self.menu = menu
        self.compact = compact
        self.css = css
        self.pretty = pretty

    ################################################################################################################################
    def process(self):
        """Render the page."""

        var_store = self.manifest.var_store
        render = self.manifest.render_get(RENDER_HTML)

        title = xml_node_attribute(self.root, "title", True)
        subtitle = xml_node_attribute(self.root, "subtitle")

        tagline = var_store.get("project-tagline")

        builder = HtmlBuilder(
            var_store.replace_str("{[project]}" + ("" if tagline is None else " - " + tagline)),
            var_store.replace_str(title + ("" if subtitle is None else " - " + subtitle)),
            var_store.get("project-favicon"),
            None if var_store.test("logo", "n") else var_store.get("project-logo"),
            var_store.replace_str(xml_node_field(self.root, "description", True).strip()),
            self.pretty,
            self.compact,
            self.css if self.compact else None,
        )

        header = builder.body.add_new("div", "page-header")

        if var_store.get("html-logo") is not None:
            header.add_new("div", "page-header-logo", content="{[html-logo]}")

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
            toc = builder.body.add_new("div", "page-toc")
            toc.add_new("div", "page-toc-header").add_new("div", "page-toc-title", content="Table of Contents")
            toc_body = toc.add_new("div", "page-toc-body")

        body = builder.body.add_new("div", "page-body")

        for section_no, section in enumerate(xml_node_child_list(self.root, "section")):
            element, toc_element = self._section_process(section, None, str(section_no + 1), 1)

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
        element = HtmlElement("div", "section%d" % depth)

        element.add_new("a", id=anchor)

        header = element.add_new("div", "section%d-header" % depth)
        title = self.process_text(xml_node_child(section, "title", True))

        if self.toc_number:
            header.add_new("div", "section%d-number" % depth)

        header.add_new("div", "section%d-title" % depth, content=title)

        if self.toc_number:
            toc_element.add_new("div", "section%d-toc-number" % depth)

        toc_element.add_new("div", "section%d-toc-title" % depth).add_new("a", content=title, ref="#%s" % anchor)

        text = xml_node_text(section)

        if text is not None:
            element.add_new("div", "section-intro", content=self.process_text(text))

        body = element.add_new("div", "section-body")
        child_no = 1

        for child in child_list(section):
            log(DEBUG, "    " * (depth + 2) + "process child %s" % child.tag)

            if child.tag == "execute-list":
                self._execute_list_process(section, child, body, depth)
            elif child.tag == "code-block":
                body.add_new("pre", "code-block", content=_code_block(child), pre=True)
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

            execute_body.add_new("pre", "execute-body-cmd", content=cmd.replace("\n", "\n   "), pre=True)

            highlight = self.manifest.var_store.replace_str(xml_node_field(command, "exe-highlight"))
            found = False

            if output is not None:
                if xml_node_field(command, "exe-highlight-type") == "error":
                    expect_error = True

                # Runs of lines are grouped by whether they are highlighted, so a run is one element rather than one per line
                previous = None
                run = None

                for line in output.split("\n"):
                    highlighted = highlight is not None and re.search(highlight, line) is not None

                    if previous is not None and highlighted != previous:
                        execute_body.add_new("pre", _output_class(previous, expect_error), content=run, pre=True)
                        run = None

                    run = line if run is None else run + "\n" + line
                    previous = highlighted
                    found = found or highlighted

                # Whatever is left is the last run, since output always holds at least one line
                execute_body.add_new("pre", _output_class(previous, expect_error), content=run, pre=True)

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

        element.add_new(
            "div",
            "config-title",
            content='<span class="host">%s</span>:<span class="file">%s</span> <b>&#x21d2;</b> %s'
            % (host_name, file, self.process_text(xml_node_child(node, "title", True))),
        )

        body = element.add_new("div", "config-body")

        body.add_new(
            "div",
            "config-body-output",
            content=("<No PgBackRest Settings>" if config is None else config.replace("\n", "<br/>\n")),
        )

        return element


####################################################################################################################################
def _output_class(highlighted, expect_error):
    """What a run of output is called, which is what decides how it looks."""

    if not highlighted:
        return "execute-body-output"

    return "execute-body-output-highlight" + ("-error" if expect_error else "")


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
    path_css = os.path.join(path_doc, "resource/html/default.css")
    var_store = manifest.var_store

    if not render.compact:
        shutil.copyfile(path_css, os.path.join(path_out, "default.css"))

        favicon = var_store.get("project-favicon")

        if favicon is not None:
            shutil.copyfile(os.path.join(path_doc, "resource", favicon), os.path.join(path_out, favicon))

        logo = var_store.get("project-logo")

        if logo is not None and not var_store.test("logo", "n"):
            shutil.copyfile(os.path.join(path_doc, "resource", logo), os.path.join(path_out, logo))

        if not var_store.test("sponsor", "n"):
            path_sponsor = os.path.join(path_doc, "resource/sponsor")
            path_sponsor_out = os.path.join(path_out, "sponsor")

            path_create(path_sponsor_out)

            for name in path_list(path_sponsor):
                shutil.copyfile(os.path.join(path_sponsor, name), os.path.join(path_sponsor_out, name))

    css = file_read(path_css)

    for key in sorted(render.out_map):
        log(INFO, "    render out: %s" % key)

        out = render.out_map[key]

        def build():
            return var_store.replace_str(DocHtmlPage(manifest, key, render.menu, exe, render.compact, css, render.pretty).process())

        try:
            html = build()
        except CacheInvalidError:
            # The cache no longer describes the document, so throw it away and build the document for real
            manifest.cache_reset(out.source)
            html = build()

        file_write(os.path.join(path_out, out.file if out.file is not None else "%s.html" % key), html)
