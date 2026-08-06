"""Render Help.

Writes the help the binary carries for itself. The text is rendered from the same xml the documentation is built from, packed, and
compressed, so the help for every command and option costs about as much as one page of it would uncompressed.

The result is a C string literal rather than a list of byte values, which is far more compact, and the data is not terminated so the
array is declared as not being a string and sized to exactly what is there."""

####################################################################################################################################
import bz2
import os

from common.error import ToolError
from common.pack import PackWrite
from common.render import LINE_LENGTH, bld_header
from common.storage import file_write, file_write_differs

_MODULE = "help"
_DESCRIPTION = "Help Data"

# Compression level, which is the most bzip2 offers since the data is compressed once and read forever
_COMPRESS_LEVEL = 9

# Tags that render as the text they stand for
_TAG_TEXT = {"backrest": "pgBackRest", "postgres": "PostgreSQL"}

# Tags that render as what they contain, followed by what they add. A tag that adds nothing renders as its content alone.
_TAG_WRAP = {
    "admonition": ("NOTE: ", "\n\n"),
    "list": ("", "\n"),
    "list-item": ("* ", "\n"),
    "p": ("", "\n\n"),
    "quote": ('"', '"'),
}

# Tags that render as what they contain
_TAG_CONTENT = (
    "b",
    "br-option",
    "cmd",
    "code",
    "exe",
    "file",
    "host",
    "i",
    "id",
    "link",
    "path",
    "pg-setting",
    "proper",
    "setting",
)

# Bytes that have a short escape in C, which are written as that rather than as an octal escape
_ESCAPE = {
    0x07: "\\a",
    0x08: "\\b",
    0x09: "\\t",
    0x0A: "\\n",
    0x0B: "\\v",
    0x0C: "\\f",
    0x0D: "\\r",
    ord('"'): '\\"',
    ord("\\"): "\\\\",
    ord("?"): "\\?",
}


####################################################################################################################################
def _render_text(text):
    """Render a run of text.

    Text that runs over more than one line is how the xml is laid out to be read rather than something to render, so it is dropped
    once it is known to be only whitespace."""

    if text is None:
        return ""

    if "\n" in text:
        if text.strip() != "":
            raise ToolError("text '%s' is invalid" % text)

        return ""

    # The dash is written as a variable in the xml so it does not get treated as markup by the documentation renderers
    return text.replace("{[dash]}", "-")


####################################################################################################################################
def _render_node(node):
    """Render the content of a node as console text."""

    result = _render_text(node.text)

    for child in node:
        if child.tag in _TAG_TEXT:
            result += _TAG_TEXT[child.tag]
        elif child.tag in _TAG_WRAP:
            begin, end = _TAG_WRAP[child.tag]
            result += begin + _render_node(child) + end
        elif child.tag in _TAG_CONTENT:
            result += _render_node(child)
        else:
            raise ToolError("unknown tag '%s'" % child.tag)

        result += _render_text(child.tail)

    return result


####################################################################################################################################
def bld_hlp_render_xml(node):
    """Render a help node as the console text the help shows."""

    return _render_node(node).strip()


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list or []:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _pack(bld_cfg, bld_hlp):
    """Pack the help, which is one entry per command and one per option, in the order the code looks them up in."""

    pack = PackWrite()

    pack.array_begin()

    for cmd in bld_cfg.cmd_list:
        cmd_hlp = _find(bld_hlp.cmd_list, cmd.name)

        pack.bool_write(cmd.internal)
        pack.str_write(bld_hlp_render_xml(cmd_hlp.summary))
        pack.str_write(bld_hlp_render_xml(cmd_hlp.description))

    pack.array_end()

    pack.array_begin()

    for opt in bld_cfg.opt_list:
        opt_hlp = _find(bld_hlp.opt_list, opt.name)

        pack.bool_write(opt.internal)

        # An option documented only by the commands that take it has no help of its own here
        pack.str_write(opt_hlp.section if opt_hlp is not None else None)
        pack.str_write(bld_hlp_render_xml(opt_hlp.summary) if opt_hlp is not None else None)
        pack.str_write(bld_hlp_render_xml(opt_hlp.description) if opt_hlp is not None else None)

        # Deprecated names, less any that is the option name itself since that is not a name to report as deprecated
        deprecate_list = [deprecate.name for deprecate in opt.deprecate_list or [] if deprecate.name != opt.name]

        if len(deprecate_list) > 0:
            pack.array_begin()

            for name in deprecate_list:
                pack.str_write(name)

            pack.array_end()
        else:
            pack.null_write()

        # What a command says about the option that differs from what the option says, i.e. its own help or being internal
        found = False

        for cmd_idx, cmd in enumerate(bld_cfg.cmd_list):
            opt_cmd = _find(opt.cmd_list, cmd.name)

            if opt_cmd is None:
                continue

            cmd_opt_hlp = _find(_find(bld_hlp.cmd_list, cmd.name).opt_list, opt.name)

            if opt.internal == opt_cmd.internal and cmd_opt_hlp is None:
                continue

            if not found:
                pack.array_begin()
                found = True

            pack.obj_begin(id=cmd_idx + 1)

            if opt.internal != opt_cmd.internal:
                pack.bool_write(opt_cmd.internal, default_write=True)
            else:
                pack.null_write()

            if cmd_opt_hlp is not None:
                pack.str_write(bld_hlp_render_xml(cmd_opt_hlp.summary))
                pack.str_write(bld_hlp_render_xml(cmd_opt_hlp.description))

            pack.obj_end()

        if found:
            pack.array_end()
        else:
            pack.null_write()

    pack.array_end()

    return pack.end()


####################################################################################################################################
def _bld_hlp_data(bld_cfg, bld_hlp):
    """The packed and compressed help, which is what the binary carries and decompresses at run time."""

    return bz2.compress(_pack(bld_cfg, bld_hlp), _COMPRESS_LEVEL)


####################################################################################################################################
def _render_help_auto_c(bld_cfg, bld_hlp):
    """Render help.auto.c.inc, which is the compressed help as a C string literal."""

    data = _bld_hlp_data(bld_cfg, bld_hlp)

    result = bld_header(_MODULE, _DESCRIPTION)
    result += 'VR_NON_STRING static const char helpData[%u] =\n"' % len(data)

    # Break the physical lines at a fixed width with a continuation, which the preprocessor removes before the string is tokenized.
    # Breaking here rather than between escapes keeps every line the same width even when the break falls inside an escape sequence.
    # The opening quote takes the first column of the first line.
    line = [""]
    line_size = 1

    for byte in data:
        escape = _ESCAPE.get(byte, chr(byte) if 0x20 <= byte <= 0x7E else "\\%03o" % byte)

        for char in escape:
            # Counting the continuation the line is as wide as the rest of the source
            if line_size == LINE_LENGTH - 1:
                line.append("")
                line_size = 0

            line[-1] += char
            line_size += 1

    return result + "\\\n".join(line) + '";\n'


####################################################################################################################################
def bld_hlp_render(path_build, bld_cfg, bld_hlp):
    """Render the help."""

    file_write_differs(os.path.join(path_build, "src/command/help/help.auto.c.inc"), _render_help_auto_c(bld_cfg, bld_hlp))


####################################################################################################################################
def bld_hlp_render_data(path_build, bld_cfg, bld_hlp):
    """Write the help as raw data rather than as C.

    The help unit test loads this to run the help against the current declarations, since the C literal the build compiles in may not
    have been generated when the test runs. It goes at the root of the build path because it is not part of any source tree."""

    file_write(os.path.join(path_build, "help.dat"), _bld_hlp_data(bld_cfg, bld_hlp))
