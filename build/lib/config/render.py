"""Render Configuration Data.

Writes the constants and enums the code refers to options and commands by, and the parse rules the command line and configuration
file are read with.

The parse rules are packed rather than written as plain structures: every value the rules refer to is collected into one array per
type and the rules index into it, so a value used by many options is stored once. That is what most of the work here is. Each rule
is also labelled with the option or command it came from, in a comment at the right margin, so the generated file can be read.

Rules that come out identical for several commands are emitted once with a filter naming those commands, which is why the rules are
built as text first and grouped by what they turned out to be."""

####################################################################################################################################
import os

from common.error import ToolError, check
from common.render import COMMENT_SEPARATOR, LINE_LENGTH, bld_comment_block, bld_define, bld_enum, bld_header, bld_str_id_seq
from common.storage import file_write_differs
from common.string_id import STRING_ID_SEQ_NONE
from config.common import cfg_parse_size, cfg_parse_time
from config.parse import (
    CMD_ROLE_LIST,
    DEFAULT_TYPE_DYNAMIC,
    DEFAULT_TYPE_LITERAL,
    OPT_TYPE_BOOLEAN,
    OPT_TYPE_HASH,
    OPT_TYPE_INTEGER,
    OPT_TYPE_LIST,
    OPT_TYPE_PATH,
    OPT_TYPE_SIZE,
    OPT_TYPE_STRING,
    OPT_TYPE_STRING_ID,
    OPT_TYPE_TIME,
)

_MODULE = "config"
_CONFIG_DESCRIPTION = "Command and Option Configuration"
_PARSE_DESCRIPTION = "Config Parse Rules"

# Characters that cannot appear in a C name, and what they are spelled as when a value becomes one
_ENUM_CHAR = {'"': "QT", "/": "FS", " ": "SP", ".": "DT", "-": "DS"}

# Optional rules, in the order they are rendered in. A rule declared for a command replaces the one declared for the option, so both
# are collected under the same key and the command wins.
_RULE_DEPEND = "01-depend"
_RULE_SEQUENCE = "02-sequence"
_RULE_ALLOW_RANGE = "03-allow-range"
_RULE_ALLOW_LIST = "04-allow-list"
_RULE_DEFAULT = "05-default"
_RULE_REQUIRE = "06-require"

_RULE_LIST = (_RULE_DEPEND, _RULE_SEQUENCE, _RULE_ALLOW_RANGE, _RULE_ALLOW_LIST, _RULE_DEFAULT, _RULE_REQUIRE)


####################################################################################################################################
def _const(prefix, value):
    """Build a constant name from a value, e.g. "repo-path" becomes "CFGOPT_REPO_PATH"."""

    return ("%s_%s" % (prefix, value)).replace("-", "_").upper()


####################################################################################################################################
def _enum_str(source):
    """Build the part of a C name that stands for a value.

    A value can hold characters a name cannot, so each is spelled out and separated from what surrounds it, e.g. "9.6" becomes
    "9_DT_6" and "\"none\"" becomes "QT_none_QT"."""

    result = ""
    prior_special = False

    for idx, char in enumerate(source):
        if char not in _ENUM_CHAR:
            result += char
            prior_special = False

            continue

        if idx != 0 and not prior_special:
            result += "_"

        result += _ENUM_CHAR[char]

        if idx != len(source) - 1:
            result += "_"

        prior_special = True

    return result


####################################################################################################################################
def _var_128_size(value):
    """Bytes a value takes when it is encoded as a variable length integer, which is how a rule stores an index."""

    result = 1

    while value >= 0x80:
        value >>= 7
        result += 1

    return result


####################################################################################################################################
def _label(content, label, label_text):
    """Label every line of a rule with what it came from, in a comment at the right margin.

    A line that is already too long to fit the comment is left alone, since wrapping it would be worse than leaving it unlabelled.
    """

    if not label:
        return content

    comment = "// %s" % label_text
    result = []

    for line in content.split("\n"):
        result.append(line if len(line) + 1 + len(comment) > LINE_LENGTH else line + comment.rjust(LINE_LENGTH - len(line)))

    return "\n".join(result)


####################################################################################################################################
def _scalar(scalar, opt_type):
    """Render a value as the rule that refers to it, which is an index into the values of its type."""

    if opt_type == OPT_TYPE_STRING_ID:
        return "PARSE_RULE_VAL_STRID(%s)" % _enum_str(scalar)

    if opt_type == OPT_TYPE_STRING:
        return "PARSE_RULE_VAL_STR(%s)" % _enum_str(scalar)

    if opt_type == OPT_TYPE_BOOLEAN:
        return "PARSE_RULE_VAL_BOOL_%s" % ("TRUE" if scalar == "true" else "FALSE")

    if opt_type == OPT_TYPE_INTEGER:
        return "PARSE_RULE_VAL_INT(%s)" % _enum_str(scalar)

    if opt_type == OPT_TYPE_SIZE:
        return "PARSE_RULE_VAL_SIZE(%s)" % scalar

    return "PARSE_RULE_VAL_TIME(%s)" % scalar


####################################################################################################################################
def _render_depend(opt_type, depend):
    """Render what an option depends on, i.e. the option and the values of it that make this option valid."""

    result = "                PARSE_RULE_OPTIONAL_DEPEND\n                (\n"

    if depend.default_value is not None:
        result += "                    PARSE_RULE_OPTIONAL_DEPEND_DEFAULT(%s),\n" % _scalar(depend.default_value, opt_type)

    result += "                    PARSE_RULE_VAL_OPT(%s),\n" % bld_enum("", depend.option.name)

    if depend.value_list is not None:
        # A string option is compared as a StringId here, since a dependency is on one of a small set of known values
        type_depend = OPT_TYPE_STRING_ID if depend.option.type == OPT_TYPE_STRING else depend.option.type

        for value in depend.value_list:
            result += "                    %s,\n" % _scalar(value, type_depend)

    return result + "                )"


####################################################################################################################################
def _render_allow_range(allow_range, opt_type):
    """Render the range a value must be in, which may be a range per value of the option the range maps on."""

    result = "                PARSE_RULE_OPTIONAL_ALLOW_RANGE\n                (\n"

    if allow_range.map_list is not None:
        result += "                    PARSE_RULE_OPTIONAL_ALLOW_RANGE_MAP\n                    (\n"

        for map in allow_range.map_list:
            result += "                        %s,\n" % _scalar(map.map, OPT_TYPE_STRING_ID)
            result += "                        %s,\n" % _scalar(map.min, opt_type)
            result += "                        %s,\n" % _scalar(map.max, opt_type)

        result += "                    ),\n"
    else:
        result += "                    %s,\n" % _scalar(allow_range.min, opt_type)
        result += "                    %s,\n" % _scalar(allow_range.max, opt_type)

    return result + "                )"


####################################################################################################################################
def _render_allow_list(allow_list, opt_type):
    """Render the values an option allows.

    A value that is only compiled in with a feature is replaced by false when the feature is not, so the value keeps its index and
    the rules that refer to it by index stay correct."""

    result = "                PARSE_RULE_OPTIONAL_ALLOW_LIST\n                (\n"

    for allow in allow_list:
        value = '"%s"' % allow.value if opt_type == OPT_TYPE_STRING else allow.value

        result += "                    %s,\n" % _scalar(value, opt_type)

        if allow.condition is not None:
            result += "#ifndef %s\n                        PARSE_RULE_BOOL_FALSE,\n#endif\n" % allow.condition

    return result + "                )"


####################################################################################################################################
def _render_default_value(value, default_type, opt_type, indent):
    """Render one default value."""

    result = "    " if indent else ""

    # A string or path default is rendered as a string, quoted unless it is written as the literal C to use
    if opt_type in (OPT_TYPE_STRING, OPT_TYPE_PATH):
        quote = "" if default_type == DEFAULT_TYPE_LITERAL else '"'
        value = _scalar("%s%s%s" % (quote, value, quote), OPT_TYPE_STRING)
    else:
        value = _scalar(value, opt_type)

    return result + "                    %s,\n" % value


####################################################################################################################################
def _render_default(default, default_type, opt_type, sequence, allow_list):
    """Render the default of an option, which may be a value per value of the option the default maps on."""

    result = "                PARSE_RULE_OPTIONAL_DEFAULT\n                (\n"

    # A dynamic default is worked out at run time, so only the name of what works it out is rendered
    if default_type == DEFAULT_TYPE_DYNAMIC:
        result += "                    PARSE_RULE_DEFAULT_DYNAMIC(%s),\n" % bld_enum("", default.value)
    elif default.value is not None:
        result += _render_default_value(default.value, default_type, opt_type, False)

        # An option with a sequence also needs where the default sits in the allow list, since that is what the sequence counts
        if sequence:
            allow_idx = next((idx for idx, allow in enumerate(allow_list) if allow.value == default.value), None)

            if allow_idx is None:
                raise ToolError("unable to find default '%s' in allow list" % default.value)

            if allow_idx != 0:
                result += "                    PARSE_RULE_VAL_SEQ(%u),\n" % allow_idx
    else:
        result += "                    PARSE_RULE_OPTIONAL_DEFAULT_MAP\n                    (\n"

        for map in default.map_list:
            result += "                        %s,\n" % _scalar(map.map, OPT_TYPE_STRING_ID)
            result += _render_default_value(map.value, default_type, opt_type, True)

        result += "                    ),\n"

    return result + "                )"


####################################################################################################################################
def _value_add(opt_type, default_type, value, rule_val_map):
    """Record a value so it gets an entry in the values of its type, which is what the rules index into."""

    # A dynamic default is worked out at run time so there is no value to store
    if default_type == DEFAULT_TYPE_DYNAMIC:
        return

    # A path is stored as a string
    if opt_type == OPT_TYPE_PATH:
        opt_type = OPT_TYPE_STRING

    quote = "" if default_type == DEFAULT_TYPE_LITERAL else '"'
    value_str = "%s%s%s" % (quote, value, quote)

    # A value of any other type also has a string, since a value is reported as the text it was written as
    if opt_type != OPT_TYPE_STRING:
        rule_val_map.setdefault(opt_type, {})[value] = value_str

    rule_val_map.setdefault(OPT_TYPE_STRING, {})[value_str] = None


####################################################################################################################################
def _value_render(opt_type, rule_val_map, label, type, caption, abbr, macro, comment):
    """Render the values of one type, the map from each to its string, and the enum the rules index them by."""

    # Every type needs a value, since the array the rules index into cannot be empty
    check(opt_type in rule_val_map, "declaration has no %s value for the rules to index" % opt_type)

    rule_val_list = list(rule_val_map[opt_type])

    # Sort by what the value means rather than by how it is written, so a range check on the index is a range check on the value
    if opt_type == OPT_TYPE_INTEGER:
        rule_val_list.sort(key=lambda value: int(value))
    elif opt_type == OPT_TYPE_SIZE:
        rule_val_list.sort(key=cfg_parse_size)
    elif opt_type == OPT_TYPE_TIME:
        rule_val_list.sort(key=cfg_parse_time)
    else:
        rule_val_list.sort()

    result = "\n" + bld_comment_block("Rule %ss" % caption)
    result += (
        bld_define(
            "PARSE_RULE_VAL_%s(value)" % macro,
            "PARSE_RULE_U32_%u(parseRuleVal%s##value)" % (_var_128_size(len(rule_val_list) - 1), abbr),
        )
        + "\n"
    )

    # A StringId is stored as the string it spells, so it refers to the string values
    if opt_type == OPT_TYPE_STRING:
        result += bld_define("PARSE_RULE_VAL_STRID(value)", "PARSE_RULE_VAL_STR(QT_##value##_QT)") + "\n"

    result += "\nstatic const %s parseRuleValue%s[] =\n{\n" % (type, abbr)

    value_list = []

    for value in rule_val_list:
        if opt_type == OPT_TYPE_STRING:
            value_list.append("    PARSE_RULE_STRPUB(%s)," % value)
        elif opt_type == OPT_TYPE_INTEGER:
            value_list.append("    %s," % value.strip())
        elif opt_type == OPT_TYPE_SIZE:
            value_list.append("    %d," % cfg_parse_size(value))
        else:
            value_list.append("    %d," % cfg_parse_time(value))

    result += _label("\n".join(value_list), label, comment) + "\n};\n"

    # Map each value to its string, so a value can be reported as the text it was written as
    if opt_type in (OPT_TYPE_INTEGER, OPT_TYPE_SIZE, OPT_TYPE_TIME):
        result += "\nstatic const uint8_t parseRuleValue%sStrMap[] =\n{\n" % abbr

        map_list = ["    parseRuleValStr%s," % _enum_str(rule_val_map[opt_type][value]) for value in rule_val_list]

        result += _label("\n".join(map_list), label, "%s/strmap" % comment) + "\n};\n"

    result += "\ntypedef enum\n{\n"

    enum_list = []

    for value in rule_val_list:
        name = _enum_str(value) if opt_type in (OPT_TYPE_STRING, OPT_TYPE_INTEGER) else bld_enum("", value)
        enum_list.append("    parseRuleVal%s%s," % (abbr, name))

    result += _label("\n".join(enum_list), label, "%s/enum" % comment)

    return result + "\n} ParseRuleValue%s;\n" % abbr


####################################################################################################################################
def _render_config_auto_h(bld_cfg):
    """Render config.auto.h, which is what the code refers to commands, options, and option values by."""

    result = bld_header(_MODULE, _CONFIG_DESCRIPTION) + "#ifndef CONFIG_CONFIG_AUTO_H\n#define CONFIG_CONFIG_AUTO_H\n"

    # Command constants
    result += "\n" + bld_comment_block("Command constants")

    for cmd in bld_cfg.cmd_list:
        result += bld_define(_const("CFGCMD", cmd.name), '"%s"' % cmd.name) + "\n"

    result += "\n" + bld_define("CFG_COMMAND_TOTAL", "%u" % len(bld_cfg.cmd_list)) + "\n"

    # Option group constants
    result += "\n" + bld_comment_block("Option group constants")
    result += bld_define("CFG_OPTION_GROUP_TOTAL", "%u" % len(bld_cfg.opt_grp_list)) + "\n"

    # Option constants. An option in a group is referred to by the group and an index rather than by name.
    result += "\n" + bld_comment_block("Option constants")

    for opt in bld_cfg.opt_list:
        if opt.group is None:
            result += bld_define(_const("CFGOPT", opt.name), '"%s"' % opt.name) + "\n"

    result += "\n" + bld_define("CFG_OPTION_TOTAL", "%u" % len(bld_cfg.opt_list)) + "\n"

    # Option value constants
    result += "\n" + bld_comment_block("Option value constants")

    lf = False

    for opt in bld_cfg.opt_list:
        if opt.type != OPT_TYPE_STRING_ID:
            continue

        # Values the option allows for every command, followed by the values it allows for one command only
        value_list = []
        group_list = [("", value_list)]

        for allow in opt.allow_list or []:
            if allow.value not in value_list:
                value_list.append(allow.value)

        for opt_cmd in opt.cmd_list:
            if opt_cmd.allow_list is None:
                continue

            cmd_value_list = []

            for allow in opt_cmd.allow_list:
                # A sequence numbers the values, so the command keeps its own list rather than adding to the option's
                if opt_cmd.sequence:
                    cmd_value_list.append(allow.value)
                elif allow.value not in value_list:
                    value_list.append(allow.value)

            group_list.append(("%s_" % opt_cmd.name.upper(), cmd_value_list))

        if not opt.sequence:
            value_list.sort()

        for group_idx, (group, allow_list) in enumerate(group_list):
            sequence = (group_idx == 0 and opt.sequence) or (group_idx != 0 and len(allow_list) > 0)

            if lf and len(allow_list) > 0:
                result += "\n"

            for allow_idx, allow in enumerate(allow_list):
                const = _const("CFGOPTVAL", "%s%s_%s" % (group, opt.name, allow))

                if sequence:
                    result += bld_define(const, "%u" % allow_idx) + "\n"

                result += (
                    bld_define(
                        "%s%s" % (const, "_STRID" if sequence else ""),
                        bld_str_id_seq(allow, allow_idx if sequence else STRING_ID_SEQ_NONE),
                    )
                    + "\n"
                )

                result += bld_define("%s_Z" % const, '"%s"' % allow) + "\n"

                lf = True

    # Enums
    result += "\n" + bld_comment_block("Command enum") + "typedef enum\n{\n"
    result += "".join("    %s,\n" % bld_enum("cfgCmd", cmd.name) for cmd in bld_cfg.cmd_list)
    result += "} ConfigCommand;\n"

    result += "\n" + bld_comment_block("Option group enum") + "typedef enum\n{\n"
    result += "".join("    %s,\n" % bld_enum("cfgOptGrp", opt_grp.name) for opt_grp in bld_cfg.opt_grp_list)
    result += "} ConfigOptionGroup;\n"

    result += "\n" + bld_comment_block("Option enum") + "typedef enum\n{\n"
    result += "".join("    %s,\n" % bld_enum("cfgOpt", opt.name) for opt in bld_cfg.opt_list)
    result += "} ConfigOption;\n"

    return result + "\n#endif\n"


####################################################################################################################################
def _render_option(opt, bld_cfg, rule_val_map, dynamic_default_list):
    """Render the parse rules for one option."""

    if opt.default_type == DEFAULT_TYPE_DYNAMIC and opt.default_value.value not in dynamic_default_list:
        dynamic_default_list.append(opt.default_value.value)

    result = '    PARSE_RULE_OPTION\n    (\n        PARSE_RULE_OPTION_NAME("%s"),\n' % opt.name
    result += "        PARSE_RULE_OPTION_TYPE(%s),\n" % bld_enum("", opt.type)

    if opt.sequence:
        result += "        PARSE_RULE_OPTION_SEQUENCE(true),\n"

    # An internal option is not documented and is not shown in the help
    if opt.internal:
        result += "        PARSE_RULE_OPTION_INTERNAL(true)\n"

    if opt.default_type == DEFAULT_TYPE_DYNAMIC:
        result += "        PARSE_RULE_OPTION_DEFAULT_TYPE(Dynamic),\n"

    if opt.bool_like:
        result += "        PARSE_RULE_OPTION_BOOL_LIKE(true),\n"

    if opt.beta:
        result += "        PARSE_RULE_OPTION_BETA(true),\n"

    if opt.negate:
        result += "        PARSE_RULE_OPTION_NEGATE(true),\n"

    if opt.reset:
        result += "        PARSE_RULE_OPTION_RESET(true),\n"

    result += "        PARSE_RULE_OPTION_REQUIRED(%s),\n" % ("true" if opt.required else "false")
    result += "        PARSE_RULE_OPTION_SECTION(%s),\n" % bld_enum("", opt.section)

    if opt.secure:
        result += "        PARSE_RULE_OPTION_SECURE(true),\n"

    # A hash or a list can be given more than once, which builds up the value rather than replacing it
    if opt.type in (OPT_TYPE_HASH, OPT_TYPE_LIST):
        result += "        PARSE_RULE_OPTION_MULTI(true),\n"

    if opt.group is not None:
        result += "        PARSE_RULE_OPTION_GROUP_ID(%s),\n" % bld_enum("", opt.group)

    # An option in a group can be given without an index when a deprecation says so, e.g. repo-path for repo1-path
    for deprecate in opt.deprecate_list or []:
        if deprecate.name == opt.name and deprecate.unindexed:
            result += "        PARSE_RULE_OPTION_DEPRECATE_MATCH(true),\n"
            break

    # Commands the option is internal for, when that differs from the option itself
    cmd_internal = "".join(
        "            PARSE_RULE_OPTION_COMMAND_INTERNAL(%s, %s),\n" % (bld_enum("", cmd.name), "true" if cmd.internal else "false")
        for cmd in opt.cmd_list
        if cmd.internal != opt.internal
    )

    if cmd_internal != "":
        result += "\n        PARSE_RULE_OPTION_COMMAND_INTERNAL_LIST\n        (\n%s        )\n" % cmd_internal

    # Commands the option is valid for, per role, since a role only gets the options its part of the command needs
    for role in CMD_ROLE_LIST:
        cmd_role = "".join(
            "            PARSE_RULE_OPTION_COMMAND(%s)\n" % bld_enum("", cmd.name) for cmd in opt.cmd_list if role in cmd.role_list
        )

        if cmd_role != "":
            result += "\n        PARSE_RULE_OPTION_COMMAND_ROLE_%s_VALID_LIST\n        (\n%s        ),\n" % (role.upper(), cmd_role)

    # Rules that apply to the option whatever the command
    rule_default = {}

    if opt.depend is not None:
        rule_default[_RULE_DEPEND] = _render_depend(opt.type, opt.depend)

    if opt.allow_range is not None:
        rule_default[_RULE_ALLOW_RANGE] = _render_allow_range(opt.allow_range, opt.type)

        if opt.allow_range.map_list is not None:
            for map in opt.allow_range.map_list:
                _value_add(opt.type, False, map.min, rule_val_map)
                _value_add(opt.type, False, map.max, rule_val_map)
        else:
            _value_add(opt.type, False, opt.allow_range.min, rule_val_map)
            _value_add(opt.type, False, opt.allow_range.max, rule_val_map)

    if opt.allow_list is not None:
        rule_default[_RULE_ALLOW_LIST] = _render_allow_list(opt.allow_list, opt.type)

        for allow in opt.allow_list:
            _value_add(opt.type, False, allow.value, rule_val_map)

    if opt.default_value is not None:
        rule_default[_RULE_DEFAULT] = _render_default(opt.default_value, opt.default_type, opt.type, opt.sequence, opt.allow_list)

        # A boolean default is rendered as the boolean itself rather than as an index into a value list
        if opt.type != OPT_TYPE_BOOLEAN:
            if opt.default_value.value is not None:
                _value_add(opt.type, opt.default_type, opt.default_value.value, rule_val_map)

            for map in opt.default_value.map_list or []:
                _value_add(opt.type, opt.default_type, map.value, rule_val_map)

    # Rules that apply only to some commands, which are the rules declared for the command falling back to the rules above
    rule_cmd = {}

    for opt_cmd in opt.cmd_list:
        rule_cmd_type = {}

        if opt_cmd.depend is not None:
            rule_cmd_type[_RULE_DEPEND] = _render_depend(opt.type, opt_cmd.depend)

        if opt_cmd.sequence != opt.sequence:
            rule_cmd_type[_RULE_SEQUENCE] = "                PARSE_RULE_OPTIONAL_%sSEQUENCE()" % (
                "" if opt_cmd.required else "NOT_"
            )

        if opt_cmd.allow_list is not None:
            rule_cmd_type[_RULE_ALLOW_LIST] = _render_allow_list(opt_cmd.allow_list, opt.type)

            for allow in opt_cmd.allow_list:
                _value_add(opt.type, False, allow.value, rule_val_map)

        if opt_cmd.default_value is not None:
            rule_cmd_type[_RULE_DEFAULT] = _render_default(
                opt_cmd.default_value, opt.default_type, opt.type, opt.sequence, opt_cmd.allow_list
            )

            if opt.type != OPT_TYPE_BOOLEAN:
                _value_add(opt.type, opt.default_type, opt_cmd.default_value.value, rule_val_map)

        if opt_cmd.required != opt.required:
            rule_cmd_type[_RULE_REQUIRE] = "                PARSE_RULE_OPTIONAL_%sREQUIRED()" % ("" if opt_cmd.required else "NOT_")

        # A command with any rule of its own gets every rule, since the group it ends up in replaces the option rules entirely
        if len(rule_cmd_type) > 0:
            rule_cmd[opt_cmd.name] = [
                rule_cmd_type.get(rule, rule_default.get(rule))
                for rule in _RULE_LIST
                if rule in rule_cmd_type or rule in rule_default
            ]

    if len(rule_cmd) > 0 or len(rule_default) > 0:
        result += "\n        PARSE_RULE_OPTIONAL\n        (\n"

        if len(rule_cmd) > 0:
            # Commands whose rules came out the same share one group, which is why the rules are grouped by their text
            combine = {}

            for cmd_name, rule_list in rule_cmd.items():
                combine.setdefault("".join("\n%s,\n" % rule for rule in rule_list), []).append(cmd_name)

            for group_idx, (group, cmd_name_list) in enumerate(combine.items()):
                if group_idx != 0:
                    result += "\n"

                result += "            PARSE_RULE_OPTIONAL_GROUP\n            (\n"
                result += "                PARSE_RULE_FILTER_CMD\n                (\n"
                result += "".join("                    PARSE_RULE_VAL_CMD(%s),\n" % bld_enum("", cmd) for cmd in cmd_name_list)
                result += "                ),\n%s            ),\n" % group

        if len(rule_default) > 0:
            if len(rule_cmd) > 0:
                result += "\n"

            result += "            PARSE_RULE_OPTIONAL_GROUP\n            (\n"

            for rule_idx, rule in enumerate(rule_default.values()):
                result += ("\n" if rule_idx != 0 else "") + "%s,\n" % rule

            result += "            ),\n"

        result += "        ),\n"

    return result + "    ),"


####################################################################################################################################
def _render_parse_auto_c(bld_cfg, label):
    """Render parse.auto.c.inc, which is the rules the command line and configuration file are parsed with."""

    rule_val_map = {}
    dynamic_default_list = []

    # Command rules
    result = "\n" + bld_comment_block("Command parse data")
    result += (
        bld_define("PARSE_RULE_VAL_CMD(value)", "PARSE_RULE_U32_%u(cfgCmd##value)" % _var_128_size(len(bld_cfg.cmd_list) - 1))
        + "\n"
    )
    result += "\nstatic const ParseRuleCommand parseRuleCommand[CFG_COMMAND_TOTAL] =\n{\n"

    for cmd_idx, cmd in enumerate(bld_cfg.cmd_list):
        if cmd_idx != 0:
            result += COMMENT_SEPARATOR + "\n"

        rule = '    PARSE_RULE_COMMAND\n    (\n        PARSE_RULE_COMMAND_NAME("%s"),\n' % cmd.name

        if cmd.internal:
            rule += "        PARSE_RULE_COMMAND_INTERNAL(true)\n"

        if cmd.lock_required:
            rule += "        PARSE_RULE_COMMAND_LOCK_REQUIRED(true),\n"

        if cmd.lock_remote_required:
            rule += "        PARSE_RULE_COMMAND_LOCK_REMOTE_REQUIRED(true),\n"

        rule += "        PARSE_RULE_COMMAND_LOCK_TYPE(%s),\n" % bld_enum("", cmd.lock_type)

        if cmd.log_file:
            rule += "        PARSE_RULE_COMMAND_LOG_FILE(true),\n"

        rule += "        PARSE_RULE_COMMAND_LOG_LEVEL_DEFAULT(%s),\n" % bld_enum("", cmd.log_level_default)

        if cmd.parameter_allowed:
            rule += "        PARSE_RULE_COMMAND_PARAMETER_ALLOWED(true),\n"

        rule += "\n        PARSE_RULE_COMMAND_ROLE_VALID_LIST\n        (\n"
        rule += "".join("            PARSE_RULE_COMMAND_ROLE(%s)\n" % bld_enum("", role) for role in cmd.role_list)
        rule += "        ),\n    ),"

        result += _label(rule, label, "cmd/%s" % cmd.name) + "\n"

    result += "};\n"

    # Option group rules
    result += "\n" + bld_comment_block("Option group parse data")
    result += "static const ParseRuleOptionGroup parseRuleOptionGroup[CFG_OPTION_GROUP_TOTAL] =\n{\n"

    for opt_grp_idx, opt_grp in enumerate(bld_cfg.opt_grp_list):
        if opt_grp_idx != 0:
            result += COMMENT_SEPARATOR + "\n"

        rule = '    PARSE_RULE_OPTION_GROUP\n    (\n        PARSE_RULE_OPTION_GROUP_NAME("%s"),\n    ),' % opt_grp.name

        result += _label(rule, label, "opt-grp/%s" % opt_grp.name) + "\n"

    result += "};\n"

    # Option rules
    result += "\n" + bld_comment_block("Option parse data")
    result += (
        bld_define("PARSE_RULE_VAL_OPT(value)", "PARSE_RULE_U32_%u(cfgOpt##value)" % _var_128_size(len(bld_cfg.opt_list) - 1))
        + "\n"
    )
    result += "\nstatic const ParseRuleOption parseRuleOption[CFG_OPTION_TOTAL] =\n{\n"

    for opt_idx, opt in enumerate(bld_cfg.opt_list):
        if opt_idx != 0:
            result += COMMENT_SEPARATOR + "\n"

        rule = _render_option(opt, bld_cfg, rule_val_map, dynamic_default_list)

        result += _label(rule, label, "opt/%s" % opt.name) + "\n"

    result += "};\n"

    # Option deprecations, which are the old names an option can still be given by
    deprecate_list = []

    for opt in bld_cfg.opt_list:
        for deprecate in opt.deprecate_list or []:
            deprecate_list.append((deprecate, opt))

    deprecate_list.sort(key=lambda entry: entry[0].name)

    result += "\n" + bld_comment_block("Option deprecations")
    result += bld_define("CFG_OPTION_DEPRECATE_TOTAL", "%u" % len(deprecate_list)) + "\n"
    result += "\nstatic const ParseRuleOptionDeprecate parseRuleOptionDeprecate[CFG_OPTION_DEPRECATE_TOTAL] =\n{\n"

    for deprecate_idx, (deprecate, opt) in enumerate(deprecate_list):
        if deprecate_idx != 0:
            result += COMMENT_SEPARATOR + "\n"

        rule = '    {\n        .name = "%s",\n        .id = %s,\n' % (deprecate.name, bld_enum("cfgOpt", opt.name))

        if deprecate.indexed:
            rule += "        .indexed = true,\n"

        if deprecate.unindexed:
            rule += "        .unindexed = true,\n"

        rule += "    },"

        result += _label(rule, label, "opt-deprecate/%s" % opt.name) + "\n"

    result += "};\n"

    # Order the options are resolved in, since an option cannot be resolved before what it depends on
    result += "\n" + bld_comment_block("Order for option parse resolution")
    result += "static const uint8_t optionResolveOrder[] =\n{\n"

    resolve_list = ["    %s," % bld_enum("cfgOpt", opt.name) for opt in bld_cfg.opt_resolve_list]

    result += _label("\n".join(resolve_list), label, "opt-resolve-order") + "\n};\n"

    # Values the rules refer to, which are collected as the rules are rendered and so come after them
    value = _value_render(OPT_TYPE_STRING, rule_val_map, label, "StringPubConst", "String", "Str", "STR", "val/str")
    value += _value_render(OPT_TYPE_INTEGER, rule_val_map, label, "int", "Int", "Int", "INT", "val/int")
    value += _value_render(OPT_TYPE_SIZE, rule_val_map, label, "int64_t", "Size", "Size", "SIZE", "val/size")
    value += _value_render(OPT_TYPE_TIME, rule_val_map, label, "unsigned int", "Time", "Time", "TIME", "val/time")

    value += "\n" + bld_comment_block("Dynamic default values")
    value += (
        bld_define(
            "PARSE_RULE_DEFAULT_DYNAMIC(value)",
            "PARSE_RULE_U32_%u(parseRuleDefaultDynamic##value)" % _var_128_size(len(dynamic_default_list) - 1),
        )
        + "\n"
    )
    value += "\ntypedef enum\n{\n"
    value += "".join("    parseRuleDefaultDynamic%s,\n" % bld_enum("", name) for name in dynamic_default_list)
    value += "} ParseRuleDefaultDynamic;\n"

    return bld_header(_MODULE, _PARSE_DESCRIPTION) + value + result


####################################################################################################################################
def bld_cfg_render(path_build, bld_cfg, label):
    """Render the configuration files."""

    file_write_differs(os.path.join(path_build, "src/config/config.auto.h"), _render_config_auto_h(bld_cfg))
    file_write_differs(os.path.join(path_build, "src/config/parse.auto.c.inc"), _render_parse_auto_c(bld_cfg, label))
