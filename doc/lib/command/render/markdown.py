"""Markdown Renderer.

Renders a document as markdown, which is what the files in the repository are: the readme, the coding and contributing guides, and
the readme the distribution tarball ships.

Markdown has no way to say most of what html says, so a page is mostly text with the structure carried by headings and lists. Where
markdown cannot say something at all, e.g. a link to a section of another page, the link points at the website instead."""

####################################################################################################################################
import os
import re

from common.error import ToolError
from common.log import *
from common.storage import file_write
from common.xml import xml_node_attribute, xml_node_child, xml_node_child_list, xml_node_content, xml_node_field, xml_node_text
from command.render.execute import CacheInvalidError, DocExecute
from command.render.manifest import RENDER_MARKDOWN
from command.render.render import child_list

# How deep a page may be sectioned before the headings run out
_SECTION_DEPTH_MAX = 3


####################################################################################################################################
class DocMarkdownRender(DocExecute):
    """Renders one document as markdown."""

    def __init__(self, manifest, key, exe):
        super().__init__(RENDER_MARKDOWN, manifest, key, exe)

    ################################################################################################################################
    def process(self):
        """Render the page."""

        result = "# " + xml_node_attribute(self.root, "title", True)
        subtitle = xml_node_attribute(self.root, "subtitle")

        if subtitle is not None:
            result += " <br/> " + subtitle

        for section in xml_node_child_list(self.root, "section"):
            result = result.strip() + "\n\n" + self._section_process(section, 1)

        return result + "\n"

    ################################################################################################################################
    def _section_process(self, section, depth):
        """Render a section and everything it holds."""

        if xml_node_attribute(section, "log") == "y":
            log(INFO, "    " * (depth + 1) + "process section: %s" % xml_node_attribute(section, "path"))

        if depth > _SECTION_DEPTH_MAX:
            raise ToolError("section depth of %d exceeds maximum" % depth)

        result = "#" * (depth + 1) + " " + self.process_text(xml_node_child(section, "title", True))
        last = None

        for child in child_list(section):
            log(DEBUG, "    " * (depth + 2) + "process child %s" % child.tag)

            if child.tag == "execute-list":
                result += self._execute_list_process(section, child, depth)
            elif child.tag == "code-block":
                if xml_node_attribute(child, "title") is not None:
                    if last is not None and last != "code-block":
                        result += "\n"

                    result += "\n_" + xml_node_attribute(child, "title") + "_:"

                result += "\n```" + (xml_node_attribute(child, "type") or "")
                result += "\n" + xml_node_content(child).strip() + "\n```"
            elif child.tag == "p":
                # A paragraph that follows a table is already separated from it by the end of the table
                if last is not None and last != "table":
                    result += "\n"

                result += "\n" + self.process_text(child)
            elif child.tag == "list":
                # A blank line separates the list from what is before it, but the items stay together so the list reads as one
                for item_idx, item in enumerate(child_list(child)):
                    result += ("\n\n- " if item_idx == 0 else "\n- ") + self.process_text(xml_node_text(item))
            elif child.tag == "sponsor-list":
                for sponsor_idx, sponsor in enumerate(child_list(child)):
                    result += " " if sponsor_idx == 0 else ", "
                    result += "[%s](%s)" % (xml_node_content(sponsor), xml_node_attribute(sponsor, "url", True))

                result += "."
            elif child.tag == "section":
                result = result.strip() + "\n\n" + self._section_process(child, depth + 1)
            elif child.tag == "table":
                result += self._table_process(child)
            elif child.tag == "admonition":
                result += "\n> **" + xml_node_attribute(child, "type", True).upper() + ":** " + self.process_text(child)
            else:
                self.section_child_process(section, child, depth + 1)

            last = child.tag

        return result

    ################################################################################################################################
    def _execute_list_process(self, section, node, depth):
        """Render the commands of a section and what they wrote."""

        show = xml_node_attribute(node, "show") != "n"
        host_name = self.manifest.var_store.replace_str(xml_node_attribute(node, "host", True))
        result = ""

        if show:
            result += "\n\n%s => %s\n```\n" % (host_name, self.process_text(xml_node_child(node, "title", True)))

        for command in xml_node_child_list(node, "execute"):
            show_command = xml_node_attribute(command, "show") != "n"
            expect_error = xml_node_attribute(command, "err-expect") is not None

            cmd, output = self.execute(section, host_name, command, indent=depth + 3, show=show and show_command)

            if not (show and show_command):
                continue

            result += cmd.replace("\n", "\n   ") + "\n"

            highlight = self.manifest.var_store.replace_str(xml_node_field(command, "exe-highlight"))
            found = False

            if output is not None:
                result += "\n--- output ---\n\n"

                if xml_node_field(command, "exe-highlight-type") == "error":
                    expect_error = True

                for line in output.split("\n"):
                    highlighted = highlight is not None and re.search(highlight, line) is not None

                    if highlighted:
                        result += "ERR" if expect_error else "-->"
                    else:
                        result += "   "

                    result += " %s\n" % line
                    found = found or highlighted

            if self.exe and self.is_required(section) and highlight is not None and not found:
                raise ToolError("unable to find a match for highlight: %s" % highlight)

        return result + "```"

    ################################################################################################################################
    def _table_process(self, node):
        """Render a table.

        Markdown has no table without a header, so a table that has none gets an empty one with every column aligned left."""

        title = xml_node_child(node, "title")
        header = xml_node_child(node, "table-header")
        column_list = [] if header is None else xml_node_child_list(header, "table-column")
        result = ""

        if title is not None:
            label = xml_node_attribute(title, "label")
            text = self.process_text(title)

            result += "\n\n**" + (text if label is None else "%s: %s" % (label, text)) + "**\n\n"
        else:
            result += "\n\n"

        header_text = "| "
        header_rule = "| "

        for column_idx, column in enumerate(column_list):
            align = xml_node_attribute(column, "align") or "left"
            last = column_idx == len(column_list) - 1

            header_text += self.process_text(column) + (" |\n" if last else " | ")
            header_rule += ":---" if align in ("left", "center") else "---"
            header_rule += "---:" if align in ("right", "center") else ""
            header_rule += " |\n" if last else " | "

        data = xml_node_child(node, "table-data", True)

        if header is None:
            for _ in xml_node_child_list(xml_node_child_list(data, "table-row")[0], "table-cell"):
                header_text += "     | "
                header_rule += ":--- | "

            header_text += "\n"
            header_rule += "\n"

        result += header_text + header_rule

        for row in xml_node_child_list(data, "table-row"):
            result += "| "
            cell_list = xml_node_child_list(row, "table-cell")

            for cell_idx, cell in enumerate(cell_list):
                result += self.process_text(cell) + (" |\n" if cell_idx == len(cell_list) - 1 else " | ")

        return result


####################################################################################################################################
def markdown_render(manifest, path_out, exe):
    """Render every page."""

    render = manifest.render_get(RENDER_MARKDOWN)

    for key in sorted(render.out_map):
        log(INFO, "    render out: %s" % key)

        out = render.out_map[key]

        def build():
            return manifest.var_store.replace_str(DocMarkdownRender(manifest, key, exe).process())

        try:
            markdown = build()
        except CacheInvalidError:
            # The cache no longer describes the document, so throw it away and build the document for real
            manifest.cache_reset(out.source)
            markdown = build()

        file_write(os.path.join(path_out, out.file if out.file is not None else "%s.md" % key), markdown)
