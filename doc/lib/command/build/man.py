"""Build Manual Page Reference.

Renders the manual page, which is the reference as plain text for a console rather than as a document. It lists every command and
every option with its summary, wrapped to a fixed width.

The wrapping is a port of what the binary does for its own help, so the manual page and `pgbackrest help` break lines the same
way."""

####################################################################################################################################
from common.error import ToolError, check
from common.xml import xml_node_attribute, xml_node_child, xml_node_content
from help.render import bld_hlp_render_xml

# Width the text is wrapped to. A fixed 80 is safe on virtually any console.
_CONSOLE_WIDTH = 80

# Section an option is listed under when it has none of its own
_SECTION_DEFAULT = "General"

# Variables the manual page resolves, which is only what it actually uses. Anything else is an error rather than text that would be
# rendered as the name of a variable.
_VAR_MAP = {"{[postgres]}": "PostgreSQL", "{[project]}": "pgBackRest"}

# What the manual page says about itself, which is the same whatever the declarations hold
_NAME = "pgBackRest"
_BIN = "pgbackrest"
_CONFIG_FILE = "/etc/pgbackrest/pgbackrest.conf"

_FILES = (
    """
FILES
  %s
  /var/lib/pgbackrest
  /var/log/pgbackrest
  /var/spool/pgbackrest
  /tmp/pgbackrest
"""
    % _CONFIG_FILE
)

_EXAMPLES = (
    """
EXAMPLES
  * Create a backup of the PostgreSQL `main` cluster:

    $ pgbackrest --stanza=main backup

    The `main` cluster should be configured in `%s`

  * Show all available backups:

    $ pgbackrest info

  * Show all available backups for a specific cluster:

    $ pgbackrest --stanza=main info

  * Show backup specific options:

    $ pgbackrest help backup
"""
    % _CONFIG_FILE
)

_SEE_ALSO = """
SEE ALSO
  /usr/share/doc/pgbackrest-doc/html/index.html
  https://pgbackrest.org
"""


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _replace(string):
    """Replace the variables the manual page uses."""

    result = string

    for variable, value in _VAR_MAP.items():
        result = result.replace(variable, value)

    if "{[" in result:
        raise ToolError("unreplaced variable(s) in: %s" % string)

    return result


####################################################################################################################################
def _split_size(string, size):
    """Split a paragraph into lines no longer than a size, breaking at spaces.

    A word longer than the size is left on a line of its own rather than broken, so a path or a url stays readable."""

    result = []
    base = 0
    match_last = None

    while True:
        match = string.find(" ", base if match_last is None else match_last)

        if match == -1:
            # Whatever is left is the last line, less a break when what is left is still too long
            if match_last is not None and len(string) - base - 1 >= size:
                result.append(string[base : match_last - 1])
                base = match_last

            result.append(string[base:])

            return result

        # A word that would take the line past the size starts the next line, breaking at the space before it when there is one
        if match - base >= size:
            if match_last is not None:
                match = match_last - 1

            result.append(string[base:match])
            base = match + 1
            match_last = None
        else:
            match_last = match + 1


####################################################################################################################################
def _render_text(text, indent, indent_first):
    """Wrap text to the console width, indenting every line but optionally the first.

    The first line is not indented where the text follows a name on the same line, e.g. a command and its summary."""

    result = ""

    for line in text.split("\n"):
        if result != "":
            result += "\n"

        for part_idx, part in enumerate(_split_size(line, _CONSOLE_WIDTH - indent)):
            if part_idx != 0 or indent_first:
                if part_idx != 0:
                    result += "\n"

                if len(part) > 0:
                    result += " " * indent

            result += part

    return result


####################################################################################################################################
def _command_list(bld_cfg, bld_hlp):
    """The commands to list, i.e. every command a reader can run."""

    cmd_cfg_map = {cmd_cfg.name: cmd_cfg for cmd_cfg in bld_cfg.cmd_list}

    return [cmd_hlp for cmd_hlp in bld_hlp.cmd_list if not cmd_cfg_map[cmd_hlp.name].internal]


####################################################################################################################################
def _section_list(bld_cfg, bld_hlp, cmd_hlp_list):
    """The options to list, grouped by section, and the width the widest of them needs.

    An option a command documents itself is listed under that command, since what it means there is what was documented."""

    # The help is checked against the declarations when it is parsed, but only that everything declared has help. Help for something
    # that is not declared is a leftover, and it is reported here rather than left to fail as a missing key.
    opt_cfg_map = {opt_cfg.name: opt_cfg for opt_cfg in bld_cfg.opt_list}

    section_map = {}
    width_max = 0

    def add(section, opt_hlp):
        section_map.setdefault(section, []).append(opt_hlp)

    def opt_cfg_find(name, where):
        result = opt_cfg_map.get(name)
        check(result is not None, "help %s documents option '%s', which is not declared" % (where, name))

        return result

    for cmd_hlp in cmd_hlp_list:
        for opt_hlp in cmd_hlp.opt_list or []:
            opt_cfg = opt_cfg_find(opt_hlp.name, "for command '%s'" % cmd_hlp.name)
            opt_cmd_cfg = _find(opt_cfg.cmd_list, cmd_hlp.name)

            check(
                opt_cmd_cfg is not None,
                "help for command '%s' documents option '%s', which the command does not take" % (cmd_hlp.name, opt_hlp.name),
            )

            # Skip an option the command marks internal, since a reader has no use for it
            if opt_cmd_cfg.internal:
                continue

            width_max = max(width_max, len(opt_hlp.name))
            add(cmd_hlp.name[:1].upper() + cmd_hlp.name[1:], opt_hlp)

    for opt_hlp in bld_hlp.opt_list:
        if opt_cfg_find(opt_hlp.name, "in the option list").internal:
            continue

        width_max = max(width_max, len(opt_hlp.name))

        section = _SECTION_DEFAULT if opt_hlp.section is None else opt_hlp.section[:1].upper() + opt_hlp.section[1:]

        add(section, opt_hlp)
        section_map[section].sort(key=lambda opt_hlp: opt_hlp.name)

    return sorted(section_map.items()), width_max


####################################################################################################################################
def reference_man_render(index, bld_cfg, bld_hlp):
    """Render the manual page."""

    # What the project is, which is taken from the documentation index rather than said again here
    subtitle = _replace(xml_node_attribute(index, "subtitle", True))
    description = _render_text(_replace(xml_node_content(xml_node_child(index, "description", True))), 2, True)

    result = "NAME\n  %s - %s\n\nSYNOPSIS\n  %s [options] [command]\n\nDESCRIPTION\n%s\n" % (
        _NAME,
        subtitle,
        _BIN,
        description,
    )

    cmd_hlp_list = _command_list(bld_cfg, bld_hlp)
    width_max = max(len(cmd_hlp.name) for cmd_hlp in cmd_hlp_list)

    result += "\nCOMMANDS\n"

    for cmd_hlp in cmd_hlp_list:
        summary = _render_text(bld_hlp_render_xml(cmd_hlp.summary), width_max + 4, False)

        result += "  %s  %*s%s\n" % (cmd_hlp.name, width_max - len(cmd_hlp.name), "", summary)

    section_list, width_max = _section_list(bld_cfg, bld_hlp, cmd_hlp_list)

    result += "\nOPTIONS\n"

    for section_idx, (section, opt_hlp_list) in enumerate(section_list):
        if section_idx != 0:
            result += "\n"

        result += "  %s Options:\n" % section

        for opt_hlp in opt_hlp_list:
            summary = _render_text(bld_hlp_render_xml(opt_hlp.summary), width_max + 8, False)

            result += "    --%s  %*s%s\n" % (opt_hlp.name, width_max - len(opt_hlp.name), "", summary)

    return result + _FILES + _EXAMPLES + _SEE_ALSO
