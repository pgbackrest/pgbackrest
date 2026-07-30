"""Build Command and Configuration Reference.

Renders the reference documents from the same declarations the binary is generated from, so what is documented is what the code does
rather than a second description of it that has to be kept in step.

The configuration reference documents every option that can be set in the configuration file, grouped by the section it belongs to. The
command reference documents every command and, under each, the options that command takes, grouped by what they are about. An option
therefore appears in both, described once and rendered twice."""

####################################################################################################################################
from common.xml import xml_document_new, xml_node_add, xml_node_attribute_set, xml_node_child_add, xml_node_content_add
from config.parse import DEFAULT_TYPE_DYNAMIC, OPT_TYPE_BOOLEAN, SECTION_COMMAND_LINE

# Role a user gives an option, as opposed to a role a command starts for itself
_CMD_ROLE_MAIN = "main"

# Section an option is documented under when it has none of its own
_SECTION_DEFAULT = "general"

# Sections that mean the same thing in the command reference as in the configuration reference. Anything else is about the command
# itself and is grouped under one heading there.
_SECTION_COMMAND_LIST = ("general", "log", "maintainer", "repository", "stanza")

_SECTION_COMMAND = "command"


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list or []:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _default_block(block_list, opt_cfg, default):
    """Describe the default of an option, which may be a value per value of the option it depends on."""

    # A dynamic default is worked out when the command runs, so what it depends on is described rather than a value
    if opt_cfg.default_type == DEFAULT_TYPE_DYNAMIC:
        block_list.append("default: [path of executed pgbackrest binary]")
    elif default is not None:
        if default.value is not None:
            # A boolean is written the way it is given on the command line rather than the way it is declared
            if opt_cfg.type == OPT_TYPE_BOOLEAN:
                block_list.append("default: %s" % ("y" if default.value == "true" else "n"))
            else:
                block_list.append("default: %s" % default.value)
        else:
            block_list.append("default (depending on %s):" % opt_cfg.depend.option.name)

            for map in default.map_list:
                block_list.append("    %s - %s" % (map.map, map.value))

            block_list.append("")


####################################################################################################################################
def _allow_range_block(block_list, opt_cfg):
    """Describe the range an option allows, which may be a range per value of the option it depends on."""

    if opt_cfg.allow_range.map_list is not None:
        block_list.append("allow range (depending on %s):" % opt_cfg.depend.option.name)

        for map in opt_cfg.allow_range.map_list:
            block_list.append("    %s - [%s, %s]" % (map.map, map.min, map.max))

        block_list.append("")
    else:
        block_list.append("allowed: [%s, %s]" % (opt_cfg.allow_range.min, opt_cfg.allow_range.max))


####################################################################################################################################
def _example_block(block_list, opt_cfg, opt_hlp, command):
    """Show how an option is given, which is on the command line for a command and in the configuration file otherwise."""

    # An option in a group is indexed, and the example shows the first index since that is what a reader will start with
    option = opt_cfg.name

    if opt_cfg.group is not None:
        option = "%s1%s" % (opt_cfg.group, opt_cfg.name[len(opt_cfg.group) :])

    output = "example: "

    for example_idx, example in enumerate(opt_hlp.example_list):
        if command:
            if example_idx != 0:
                output += " "

            output += "--"

            # A boolean is given by naming it, so turning it off is naming the negation of it
            if opt_cfg.type == OPT_TYPE_BOOLEAN and example == "n":
                output += "no-"
        # In the configuration file each example is a line of its own, since that is how it would be written
        elif example_idx != 0:
            block_list.append(output)
            output = "example: "

        output += option

        if not command or opt_cfg.type != OPT_TYPE_BOOLEAN:
            output += "=%s" % example

    block_list.append(output)


####################################################################################################################################
def _option_render(xml_section, opt_cmd_cfg, opt_cfg, opt_hlp):
    """Render one option, either as a command takes it or as the configuration file sets it."""

    xml_option = xml_node_add(xml_section, "section")
    xml_title = xml_node_add(xml_option, "title")

    xml_node_attribute_set(xml_option, "id", "option-%s" % opt_hlp.name)
    xml_node_content_add(xml_title, "%s Option (" % opt_hlp.title)
    xml_node_content_add(xml_node_add(xml_title, "id"), "--%s" % opt_hlp.name)
    xml_node_content_add(xml_title, ")")
    xml_node_child_add(xml_node_add(xml_option, "p"), opt_hlp.summary)

    if opt_cfg.beta:
        xml_node_content_add(xml_node_add(xml_option, "p"), "FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.")

    xml_node_child_add(xml_option, opt_hlp.description)

    # What a reader needs to use the option, which is rendered as one block below the description
    block_list = []
    default = (
        opt_cmd_cfg.default_value if opt_cmd_cfg is not None and opt_cmd_cfg.default_value is not None else opt_cfg.default_value
    )

    _default_block(block_list, opt_cfg, default)

    if opt_cfg.allow_range is not None:
        _allow_range_block(block_list, opt_cfg)

    if opt_hlp.example_list is not None:
        _example_block(block_list, opt_cfg, opt_hlp, opt_cmd_cfg is not None)

    if len(block_list) > 0:
        xml_node_content_add(xml_node_add(xml_option, "code-block"), "\n".join(block_list).strip())

    # Names the option can still be given by, less any that is the option name itself since that is not a deprecated name
    if opt_cfg.deprecate_list is not None:
        deprecate = ""

        for entry in opt_cfg.deprecate_list:
            if entry.name != opt_cfg.name:
                deprecate += "%s %s" % ("," if deprecate != "" else "", entry.name)

        if deprecate != "":
            xml_node_content_add(
                xml_node_add(xml_option, "p"),
                "Deprecated Name%s:%s" % ("s" if len(opt_cfg.deprecate_list) > 1 else "", deprecate),
            )


####################################################################################################################################
def _document_new(title, description, introduction):
    """Build a reference document, which begins with what it is and an introduction to it."""

    result = xml_document_new("doc", dtd_name="doc", dtd_file="doc.dtd")

    xml_node_attribute_set(result.root, "title", "{[project]}")
    xml_node_attribute_set(result.root, "subtitle", title)
    xml_node_attribute_set(result.root, "toc", "y")

    xml_node_content_add(xml_node_add(result.root, "description"), description)

    xml_intro = xml_node_add(result.root, "section")

    xml_node_attribute_set(xml_intro, "id", "introduction")
    xml_node_content_add(xml_node_add(xml_intro, "title"), "Introduction")
    xml_node_child_add(xml_intro, introduction)

    return result


####################################################################################################################################
def reference_configuration_render(bld_cfg, bld_hlp):
    """Render the configuration reference, i.e. every option that can be set in the configuration file."""

    result = _document_new(bld_hlp.opt_title, bld_hlp.opt_description, bld_hlp.opt_introduction)

    for section in bld_hlp.sct_list:
        xml_section = xml_node_add(result.root, "section")

        xml_node_attribute_set(xml_section, "id", "section-%s" % section.id)
        xml_node_content_add(xml_node_add(xml_section, "title"), "%s Options" % section.name)
        xml_node_child_add(xml_section, section.introduction)

        for opt_hlp in bld_hlp.opt_list:
            opt_cfg = _find(bld_cfg.opt_list, opt_hlp.name)

            # Skip an option documented under another section, one that cannot be set in the configuration file at all, and one that
            # is internal since a reader has no use for it
            if (opt_hlp.section or _SECTION_DEFAULT) != section.id:
                continue

            if opt_cfg.section == SECTION_COMMAND_LINE or opt_cfg.internal:
                continue

            _option_render(xml_section, None, opt_cfg, opt_hlp)

    return result


####################################################################################################################################
def _command_section(section):
    """The section an option is grouped under in the command reference.

    A section that means the same thing everywhere keeps its name. Anything else is about the command itself, so it is grouped under one
    heading rather than a heading per command."""

    if section is None:
        return _SECTION_DEFAULT

    return section if section in _SECTION_COMMAND_LIST else _SECTION_COMMAND


####################################################################################################################################
class _CommandOption:
    """An option as one command takes it, i.e. the option, what the command says about it, and the help to render."""

    def __init__(self, opt_cfg, opt_cmd_cfg, opt_hlp, section):
        self.opt_cfg = opt_cfg
        self.opt_cmd_cfg = opt_cmd_cfg
        self.opt_hlp = opt_hlp
        self.section = section


####################################################################################################################################
def _command_option_list(cmd_cfg, cmd_hlp, bld_cfg, bld_hlp):
    """The options a command takes, with the help to render for each."""

    result = []

    for opt_cfg in bld_cfg.opt_list:
        # Skip an option a reader has no use for or should not be told about
        if opt_cfg.internal or opt_cfg.secure:
            continue

        # Skip an option this command does not take, or takes only for a role the user does not start
        opt_cmd_cfg = _find(opt_cfg.cmd_list, cmd_cfg.name)

        if opt_cmd_cfg is None or opt_cmd_cfg.internal or _CMD_ROLE_MAIN not in opt_cmd_cfg.role_list:
            continue

        # Help a command documents differently wins over the help the option has of its own
        opt_hlp = _find(cmd_hlp.opt_list, opt_cfg.name)
        section = _SECTION_COMMAND if opt_hlp is not None else None

        if opt_hlp is None:
            opt_hlp = _find(bld_hlp.opt_list, opt_cfg.name)
            section = opt_hlp.section

        result.append(_CommandOption(opt_cfg, opt_cmd_cfg, opt_hlp, _command_section(section)))

    return result


####################################################################################################################################
def reference_command_render(bld_cfg, bld_hlp):
    """Render the command reference, i.e. every command and the options it takes."""

    result = _document_new(bld_hlp.cmd_title, bld_hlp.cmd_description, bld_hlp.cmd_introduction)

    for cmd_hlp in bld_hlp.cmd_list:
        cmd_cfg = _find(bld_cfg.cmd_list, cmd_hlp.name)

        # Skip a command a reader has no use for
        if cmd_cfg.internal:
            continue

        xml_section = xml_node_add(result.root, "section")
        xml_title = xml_node_add(xml_section, "title")

        xml_node_attribute_set(xml_section, "id", "command-%s" % cmd_hlp.name)
        xml_node_content_add(xml_title, "%s Command (" % cmd_hlp.title)
        xml_node_content_add(xml_node_add(xml_title, "id"), cmd_hlp.name)
        xml_node_content_add(xml_title, ")")
        xml_node_child_add(xml_section, cmd_hlp.description)

        opt_list = _command_option_list(cmd_cfg, cmd_hlp, bld_cfg, bld_hlp)

        for section in sorted({opt.section for opt in opt_list}):
            xml_category = xml_node_add(xml_section, "section")

            xml_node_attribute_set(xml_category, "id", "category-%s" % section)
            xml_node_attribute_set(xml_category, "toc", "n")
            xml_node_content_add(xml_node_add(xml_category, "title"), "%s Options" % (section[:1].upper() + section[1:]))

            for opt in opt_list:
                if opt.section == section:
                    _option_render(xml_category, opt.opt_cmd_cfg, opt.opt_cfg, opt.opt_hlp)

    return result
