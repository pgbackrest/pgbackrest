"""Parse Help Xml.

Reads the help text for every command and option. The configuration declaration says what exists and this says what it means, so the
two are checked against each other here: a command or an option the user can reach must have help, and help for something that does
not exist is a leftover.

The values an option allows are declared in the configuration as well, so the help marks the list that describes them rather than
writing the values out again as ordinary markup. The values are checked against the declaration here and the list a reader sees is
rendered from them, which is what keeps the two from drifting apart.

The help text is left as xml nodes rather than rendered, since the same nodes are rendered as console text for the help and as
documentation for the reference."""

####################################################################################################################################
from common.error import ToolError, check
from common.storage import file_read
from common.xml import (
    xml_document_new,
    xml_node_add,
    xml_node_attribute,
    xml_node_child,
    xml_node_child_add,
    xml_node_child_list,
    xml_node_child_replace,
    xml_node_content,
    xml_node_content_add,
    xml_parse,
)
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
def _allow_list_find(xml_node):
    """Every allow list under a node, with the node holding it.

    An allow list that describes each value is a list of its own but one that describes none of them is part of the sentence that
    introduces the values, so an allow list is looked for anywhere in the text rather than only at the top of it."""

    result = []

    for xml_child in xml_node:
        if xml_child.tag == "allow-list":
            result.append((xml_node, xml_child))
        else:
            result.extend(_allow_list_find(xml_child))

    return result


####################################################################################################################################
def _allow_list_inherit(xml_allow, inherit_map, name):
    """The list an allow list inherits, or the list itself when it inherits none.

    An option that allows what another option allows describes the values once and inherits them, the way the configuration
    inherits the values themselves, since the same list written out twice is two lists to keep in step.

    A list documented under a command is inherited by naming the command as well. An option that a command documents differently
    is documented once per command, so the option name alone does not say which of those lists is meant, and an option that only
    the commands document has no list of its own for the name to find."""

    inherit = xml_node_attribute(xml_allow, "inherit")

    if inherit is None:
        return xml_allow

    inherit_cmd = xml_node_attribute(xml_allow, "command")

    check(
        (inherit_cmd, inherit) in inherit_map,
        "%s inherits the allow list of option '%s'%s, which does not describe one"
        % (name, inherit, "" if inherit_cmd is None else " for command '%s'" % inherit_cmd),
    )

    return inherit_map[(inherit_cmd, inherit)]


####################################################################################################################################
def _allow_list_inherit_add(inherit_map, key, description):
    """Record the list documented in help text so an option that allows the same values can inherit it."""

    for _, xml_allow in _allow_list_find(description):
        if len(xml_node_child_list(xml_allow, "allow-item")) > 0:
            inherit_map[key] = xml_allow


####################################################################################################################################
def _allow_list_item_render(xml_node, xml_allow):
    """Render an allow list that describes each value as a list of the value and what it means.

    The list is introduced by what it is a list of, since the same sentence introduces every one of them and writing it out again
    for each is only a way for them to end up worded differently."""

    xml_node_content_add(
        xml_node_add(xml_node, "p"), "The following %s are supported:" % xml_node_attribute(xml_allow, "caption", True)
    )

    xml_list = xml_node_add(xml_node, "list")

    for xml_item in xml_node_child_list(xml_allow, "allow-item"):
        xml_list_item = xml_node_add(xml_list, "list-item")

        xml_node_content_add(xml_node_add(xml_list_item, "id"), xml_node_attribute(xml_item, "id", True))
        xml_node_content_add(xml_list_item, " - ")
        xml_node_child_add(xml_list_item, xml_item)


####################################################################################################################################
def _allow_list_value_render(xml_node, value_list):
    """Render an allow list that describes no value as the values alone, in the order they are declared."""

    for index, value in enumerate(value_list):
        # A comma separates every value but the last, which a conjunction separates and a comma as well when there are more than
        # two, since the values are read as part of the sentence that introduces them
        if index > 0:
            if index < len(value_list) - 1:
                xml_node_content_add(xml_node, ", ")
            else:
                xml_node_content_add(xml_node, ", and " if len(value_list) > 2 else " and ")

        xml_node_content_add(xml_node_add(xml_node, "id"), value)


####################################################################################################################################
def _allow_list_render(xml_text, allow_list, name, inherit_map):
    """Check an allow list documented in help text against the values the option declares and render it as a reader sees it.

    A list that describes each value says only which value each item is about, so the values are checked against the declaration
    rather than read from it, which is what catches a value that is added, removed, or renamed without the help following. Order is
    not checked because the help lists a value where a reader will look for it rather than where the configuration happens to
    declare it.

    A list that describes no value is written empty and rendered from the declaration, which is how values that speak for
    themselves are documented without writing them out again. Such a list is read as part of the sentence that introduces it, so it
    is rendered without an introduction of its own."""

    xml_allow_list = _allow_list_find(xml_text)

    # An option that allows only certain values must say what they are, since a value a user cannot read about is a value they
    # cannot use
    check(allow_list is None or len(xml_allow_list) > 0, "%s must document its allow list" % name)

    for xml_parent, xml_node in xml_allow_list:
        check(allow_list is not None, "%s does not have an allow list" % name)

        xml_allow = _allow_list_inherit(xml_node, inherit_map, name)
        allow_value_list = [allow.value for allow in allow_list]
        xml_item_list = xml_node_child_list(xml_allow, "allow-item")
        xml_replace = xml_document_new("text")

        if len(xml_item_list) > 0:
            value_list = [xml_node_attribute(xml_item, "id", True) for xml_item in xml_item_list]

            check(
                sorted(value_list) == sorted(allow_value_list),
                "%s allow list is documented as '%s' but declared as '%s'"
                % (name, ", ".join(value_list), ", ".join(allow_value_list)),
            )

            _allow_list_item_render(xml_replace, xml_allow)
        else:
            _allow_list_value_render(xml_replace, allow_value_list)

        xml_node_child_replace(xml_parent, xml_node, xml_replace)


####################################################################################################################################
def _allow_list(bld_hlp, bld_cfg):
    """Render the allow list an option documents, wherever the option is documented."""

    # The list each option describes, which is what an option that allows the same values inherits. The lists are found before any
    # of them is rendered so that what is inherited does not depend on the order the options are rendered in.
    inherit_map = {}

    for opt_hlp in bld_hlp.opt_list:
        _allow_list_inherit_add(inherit_map, (None, opt_hlp.name), opt_hlp.description)

    for cmd_hlp in bld_hlp.cmd_list:
        for opt_hlp in cmd_hlp.opt_list or []:
            _allow_list_inherit_add(inherit_map, (cmd_hlp.name, opt_hlp.name), opt_hlp.description)

    for opt in bld_cfg.opt_list:
        opt_hlp = _find(bld_hlp.opt_list, opt.name)

        if opt_hlp is not None:
            _allow_list_render(opt_hlp.description, opt.allow_list, "option '%s'" % opt.name, inherit_map)

        # An option a command documents differently is checked against what the command allows when it allows something else
        for opt_cmd in opt.cmd_list:
            opt_cmd_hlp = _find(_find(bld_hlp.cmd_list, opt_cmd.name).opt_list, opt.name)

            if opt_cmd_hlp is not None:
                _allow_list_render(
                    opt_cmd_hlp.description,
                    opt_cmd.allow_list or opt.allow_list,
                    "option '%s' for command '%s'" % (opt.name, opt_cmd.name),
                    inherit_map,
                )


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
    _allow_list(result, bld_cfg)

    return result
