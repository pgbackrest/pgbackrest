"""Parse Help Xml.

Reads the help text for every command and option. The configuration declaration says what exists and this says what it means, so the
two are checked against each other here: a command or an option the user can reach must have help, and help for something that does
not exist is a leftover.

The help text is left as xml nodes rather than rendered, since the same nodes are rendered as console text for the help and as
documentation for the reference."""

####################################################################################################################################
from common.error import ToolError, check
from common.storage import file_read
from common.xml import xml_node_attribute, xml_node_child, xml_node_child_list, xml_node_content, xml_parse
from config.parse import CMD_ROLE_MAIN


####################################################################################################################################
class BldHlpSection:
    """A section of the configuration, i.e. a group of options that are documented together."""

    def __init__(self, id, name, introduction):
        self.id = id
        self.name = name
        self.introduction = introduction


####################################################################################################################################
class BldHlpCommand:
    """Help for a command."""

    def __init__(self, name, title, summary, description, opt_list):
        self.name = name
        self.title = title
        self.summary = summary
        self.description = description
        self.opt_list = opt_list  # Help for the options this command documents differently, if any


####################################################################################################################################
class BldHlpOption:
    """Help for an option."""

    def __init__(self, name, section, title, summary, description, example_list):
        self.name = name
        self.section = section
        self.title = title
        self.summary = summary
        self.description = description
        self.example_list = example_list


####################################################################################################################################
class BldHlp:
    """The help declaration."""

    def __init__(self):
        self.sct_list = None  # Configuration sections
        self.cmd_title = None
        self.cmd_description = None
        self.cmd_introduction = None
        self.cmd_list = None
        self.opt_title = None
        self.opt_description = None
        self.opt_introduction = None
        self.opt_list = None


####################################################################################################################################
def _content(node):
    """Text a node holds, or None when the node is not there because it was not required."""

    return None if node is None else xml_node_content(node)


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list or []:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _option_list(xml_opt_list, opt_list, section_default):
    """Parse a list of option help into opt_list."""

    for xml_opt in xml_opt_list:
        example_list = [xml_node_content(example) for example in xml_node_child_list(xml_opt, "example")]

        opt_list.append(
            BldHlpOption(
                xml_node_attribute(xml_opt, "id", True),
                xml_node_attribute(xml_opt, "section") or section_default,
                xml_node_attribute(xml_opt, "name", True),
                xml_node_child(xml_opt, "summary", True),
                xml_node_child(xml_opt, "text", True),
                example_list if len(example_list) > 0 else None,
            )
        )

    opt_list.sort(key=lambda opt: opt.name)


####################################################################################################################################
def _config_section_list(xml):
    """The configuration sections, which are where the options that can be set in the configuration file are documented."""

    return xml_node_child_list(xml_node_child(xml_node_child(xml, "config", True), "config-section-list", True), "config-section")


####################################################################################################################################
def _help_option_list(xml):
    """Parse the help for every option, wherever it is documented."""

    result = []

    # Options that can be set in the configuration file, which take the section they are documented in
    for xml_section in _config_section_list(xml):
        _option_list(
            xml_node_child_list(xml_node_child(xml_section, "config-key-list", True), "config-key"),
            result,
            xml_node_attribute(xml_section, "id", True),
        )

    # Options that can only be given on the command line, which are in no section
    _option_list(
        xml_node_child_list(
            xml_node_child(xml_node_child(xml_node_child(xml, "operation", True), "operation-general", True), "option-list", True),
            "option",
        ),
        result,
        None,
    )

    return result


####################################################################################################################################
def _help_command_list(xml):
    """Parse the help for every command, and for the options a command documents differently."""

    result = []

    for xml_cmd in xml_node_child_list(xml, "command"):
        opt_list = None
        xml_opt_list = xml_node_child(xml_cmd, "option-list")

        if xml_opt_list is not None:
            opt_list = []
            _option_list(xml_node_child_list(xml_opt_list, "option"), opt_list, None)

        result.append(
            BldHlpCommand(
                xml_node_attribute(xml_cmd, "id", True),
                xml_node_attribute(xml_cmd, "name", True),
                xml_node_child(xml_cmd, "summary", True),
                xml_node_child(xml_cmd, "text", True),
                opt_list,
            )
        )

    result.sort(key=lambda cmd: cmd.name)

    return result


####################################################################################################################################
def _help_section_list(xml, detail):
    """Parse the configuration sections."""

    result = [
        BldHlpSection(
            xml_node_attribute(xml_section, "id", True),
            xml_node_attribute(xml_section, "name", True),
            xml_node_child(xml_section, "text", detail),
        )
        for xml_section in _config_section_list(xml)
    ]

    result.sort(key=lambda section: section.id)

    return result


####################################################################################################################################
def _validate(bld_hlp, bld_cfg):
    """Check the help against the configuration, since anything the user can reach needs help."""

    for cmd in bld_cfg.cmd_list:
        if _find(bld_hlp.cmd_list, cmd.name) is None:
            raise ToolError("command '%s' must have help" % cmd.name)

    for opt in bld_cfg.opt_list:
        if _find(bld_hlp.opt_list, opt.name) is not None:
            continue

        # An option documented under neither a configuration section nor the command line must be documented by every command that
        # takes it, since that is the only place left for its help to be
        for opt_cmd in opt.cmd_list:
            cmd_hlp = _find(bld_hlp.cmd_list, opt_cmd.name)
            check(cmd_hlp is not None, "command help for '%s' is missing" % opt_cmd.name)

            # Only an option a user can give needs help, i.e. one the main role of the command takes
            if CMD_ROLE_MAIN not in opt_cmd.role_list:
                continue

            if _find(cmd_hlp.opt_list, opt.name) is None:
                raise ToolError("option '%s' must have help for command '%s'" % (opt.name, opt_cmd.name))


####################################################################################################################################
def bld_hlp_parse(path_help, bld_cfg, detail):
    """Parse the help declaration.

    The path is passed in rather than built from the repository, as the other declarations are, because this one belongs to the
    documentation rather than to the build and so is not the build library's to know about.

    Detail says whether the text that only the documentation renders is required, since the help in the binary does not use it."""

    xml = xml_parse(file_read(path_help), path_help)

    xml_cfg = xml_node_child(xml, "config", True)
    xml_operation = xml_node_child(xml, "operation", True)

    result = BldHlp()
    result.sct_list = _help_section_list(xml, detail)

    result.cmd_title = xml_node_attribute(xml_operation, "title", True)
    result.cmd_description = _content(xml_node_child(xml_operation, "description", detail))
    result.cmd_introduction = xml_node_child(xml_operation, "text", detail)
    result.cmd_list = _help_command_list(xml_node_child(xml_operation, "command-list", True))

    result.opt_title = xml_node_attribute(xml_cfg, "title", True)
    result.opt_description = _content(xml_node_child(xml_cfg, "description", detail))
    result.opt_introduction = xml_node_child(xml_cfg, "text", detail)
    result.opt_list = _help_option_list(xml)

    _validate(result, bld_cfg)

    return result
