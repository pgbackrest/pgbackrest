"""Parse Configuration Yaml.

Reads the command and option declarations, which are the definition of record for the pgBackRest command line and configuration file.

Most of the work here is inheritance. An option can take another option's command list, allow list, or dependency, and a command entry
in an option can default to what the option itself declares, so what a declaration means depends on what was declared before it. That
is resolved here rather than at render time, so the render sees complete options.

The declaration order is also what the reader sees, so an option can only inherit from an option above it."""

####################################################################################################################################
import copy
import os

from common.error import ToolError, check
from common.storage import file_read
from common.yaml import YamlMap, yaml_bool, yaml_load, yaml_map_empty

# Command roles. A role is a process that carries out part of a command, and main is the process the user started. The order here is
# the order the roles are rendered in.
CMD_ROLE_MAIN = "main"
CMD_ROLE_ASYNC = "async"
CMD_ROLE_LOCAL = "local"
CMD_ROLE_REMOTE = "remote"

CMD_ROLE_LIST = (CMD_ROLE_MAIN, CMD_ROLE_ASYNC, CMD_ROLE_LOCAL, CMD_ROLE_REMOTE)

# Commands that take no options of their own, so they are left out of an option's command list
CMD_HELP = "help"
CMD_VERSION = "version"

# Option types
OPT_TYPE_BOOLEAN = "boolean"
OPT_TYPE_HASH = "hash"
OPT_TYPE_INTEGER = "integer"
OPT_TYPE_LIST = "list"
OPT_TYPE_PATH = "path"
OPT_TYPE_SIZE = "size"
OPT_TYPE_STRING = "string"
OPT_TYPE_STRING_ID = "string-id"
OPT_TYPE_TIME = "time"

# Options that the rest of the build refers to by name
OPT_STANZA = "stanza"

# Sections an option can be configured in. An option in the command-line section cannot be set in the configuration file, which is
# why it can be neither negated nor reset.
SECTION_COMMAND_LINE = "command-line"

# How a default value is rendered
DEFAULT_TYPE_QUOTE = "quote"
DEFAULT_TYPE_LITERAL = "literal"
DEFAULT_TYPE_DYNAMIC = "dynamic"

_DEFAULT_TYPE_LIST = (DEFAULT_TYPE_QUOTE, DEFAULT_TYPE_LITERAL, DEFAULT_TYPE_DYNAMIC)


####################################################################################################################################
class BldCfgCommand:
    """A command."""

    def __init__(self, name):
        self.name = name
        self.internal = False  # Is the command internal?
        self.log_file = True  # Does the command write automatically to a log file?
        self.log_level_default = "info"  # Default log level
        self.lock_required = False  # Is a lock required?
        self.lock_remote_required = False  # Is a remote lock required?
        self.lock_type = "none"  # Lock type
        self.parameter_allowed = False  # Are command line parameters allowed?
        self.role_list = None  # Roles valid for the command


####################################################################################################################################
class BldCfgOptionGroup:
    """An option group, i.e. a set of options that can be indexed, e.g. repo1-path and repo2-path."""

    def __init__(self, name):
        self.name = name


####################################################################################################################################
class BldCfgOptionValue:
    """A value in an allow list, which may be compiled in only when a feature is."""

    def __init__(self, value, condition):
        self.value = value
        self.condition = condition  # Is the value conditionally compiled?


####################################################################################################################################
class BldCfgOptionAllowRangeMap:
    """An allowed range for one value of the option the range maps on."""

    def __init__(self, map, min, max):
        self.map = map
        self.min = min
        self.max = max


####################################################################################################################################
class BldCfgOptionAllowRange:
    """An allowed range, either one range or a range per value of another option."""

    def __init__(self, min=None, max=None, map_list=None):
        self.min = min
        self.max = max
        self.map_list = map_list


####################################################################################################################################
class BldCfgOptionDefaultMap:
    """A default for one value of the option the default maps on."""

    def __init__(self, map, value):
        self.map = map
        self.value = value


####################################################################################################################################
class BldCfgOptionDefault:
    """A default value, either one value or a value per value of another option."""

    def __init__(self, value=None, map_list=None):
        self.value = value
        self.map_list = map_list


####################################################################################################################################
class BldCfgOptionDepend:
    """A dependency on another option."""

    def __init__(self, option, default_value, value_list):
        self.option = option  # Option the dependency is on
        self.default_value = default_value  # Value to use when the dependency is not resolved
        self.value_list = value_list  # Values of the option that resolve the dependency


####################################################################################################################################
class BldCfgOptionDeprecate:
    """A deprecated name for an option."""

    def __init__(self, name):
        self.name = name
        self.indexed = False  # Can the deprecation be indexed?
        self.unindexed = False  # Can the deprecation be unindexed?


####################################################################################################################################
class BldCfgOptionCommand:
    """What an option means for one command."""

    def __init__(self, name):
        self.name = name
        self.internal = None  # Is the option internal, or None to use what the option declares?
        self.required = None  # Is the option required, or None to use what the option declares?
        self.sequence = None  # Is a sequence added to the StringId, or None to use what the option declares?
        self.default_value = None
        self.depend = None
        self.allow_list = None
        self.role_list = None  # Roles of the command the option is valid for


####################################################################################################################################
class BldCfgOption:
    """An option."""

    def __init__(self, name):
        self.name = name
        self.type = None  # Option type, e.g. integer
        self.section = None  # Section the option can be configured in
        self.bool_like = False  # Does the option accept y/n and can be treated as a boolean?
        self.internal = False  # Is the option internal?
        self.beta = False  # Is the option beta?
        self.required = True  # Is the option required?
        self.negate = None  # Can the option be negated?
        self.reset = False  # Can the option be reset?
        self.sequence = False  # Is a sequence added to the StringId?
        self.default_type = DEFAULT_TYPE_QUOTE  # How the default is rendered
        self.default_value = None
        self.group = None  # Option group, if any
        self.secure = False  # Does the option contain a secret?
        self.depend = None
        self.allow_list = None
        self.allow_range = None
        self.cmd_list = None  # What the option means for each command it is valid for
        self.cmd_role_list = None  # Roles the option is valid for, before they are filtered per command
        self.deprecate_list = None


####################################################################################################################################
class BldCfg:
    """The configuration declaration."""

    def __init__(self, cmd_list, opt_grp_list, opt_list, opt_resolve_list):
        self.cmd_list = cmd_list  # Commands, sorted
        self.opt_grp_list = opt_grp_list  # Option groups, sorted
        self.opt_list = opt_list  # Options, sorted
        self.opt_resolve_list = opt_resolve_list  # Options in the order their dependencies can be resolved in


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name, or None when it is not there."""

    for item in item_list:
        if item.name == name:
            return item

    return None


####################################################################################################################################
def _command_role(role_raw, name):
    """Parse a command role list, which is a map of roles with nothing under them."""

    result = []

    for role, detail in role_raw:
        yaml_map_empty(detail, "%s role '%s'" % (name, role))
        result.append(role)

    return result


####################################################################################################################################
def _command_list(cmd_raw):
    """Parse the command list."""

    result = []

    for name, detail in cmd_raw:
        cmd = BldCfgCommand(name)

        for key, value in detail:
            if key == "command-role":
                cmd.role_list = _command_role(value, "command '%s'" % name)
            elif key == "internal":
                cmd.internal = yaml_bool(value, "command '%s' internal" % name)
            elif key == "lock-type":
                cmd.lock_type = value
            elif key == "lock-remote-required":
                cmd.lock_remote_required = yaml_bool(value, "command '%s' lock-remote-required" % name)
            elif key == "lock-required":
                cmd.lock_required = yaml_bool(value, "command '%s' lock-required" % name)
            elif key == "log-file":
                cmd.log_file = yaml_bool(value, "command '%s' log-file" % name)
            elif key == "log-level-default":
                cmd.log_level_default = value.lower()
            elif key == "parameter-allowed":
                cmd.parameter_allowed = yaml_bool(value, "command '%s' parameter-allowed" % name)
            else:
                raise ToolError("unknown command definition '%s'" % key)

        # Every command has a main role whether it declares one or not, since main is the process the user started
        cmd.role_list = sorted(set(cmd.role_list or []) | {CMD_ROLE_MAIN})

        result.append(cmd)

    return sorted(result, key=lambda cmd: cmd.name)


####################################################################################################################################
def _option_group_list(opt_grp_raw):
    """Parse the option group list, which is a map of groups with nothing under them."""

    result = []

    for name, detail in opt_grp_raw:
        yaml_map_empty(detail, "option group '%s'" % name)
        result.append(BldCfgOptionGroup(name))

    return sorted(result, key=lambda opt_grp: opt_grp.name)


####################################################################################################################################
def _allow_list(allow_raw, opt_list, name):
    """Parse an allow list, which is either the list of allowed values or the name of an option to take it from."""

    # A scalar names the option the allow list is inherited from
    if isinstance(allow_raw, str):
        check(opt_list is not None, "allow list for %s cannot be inherited" % name)

        opt_inherit = _find(opt_list, allow_raw)
        check(opt_inherit is not None, "allow list inherited from option '%s' before it is defined" % allow_raw)

        return copy.copy(opt_inherit.allow_list)

    result = []

    for value in allow_raw:
        # A scalar is the value on its own, else a map of the value to the feature it is compiled in with
        if isinstance(value, str):
            result.append(BldCfgOptionValue(value, None))
        else:
            check(len(value) == 1, "allow list value for %s must have a single condition" % name)

            for item, condition in value:
                result.append(BldCfgOptionValue(item, condition))

    return result


####################################################################################################################################
def _allow_range(range_raw, name):
    """Parse an allow range, which is either a min/max pair or a min/max pair per value of the option it maps on."""

    # A scalar first means the range itself rather than a range per map
    if isinstance(range_raw[0], str):
        check(len(range_raw) == 2, "allow range for %s must be a min and a max" % name)

        return BldCfgOptionAllowRange(min=range_raw[0], max=range_raw[1])

    map_list = []

    for entry in range_raw:
        for map, range in entry:
            check(len(range) == 2, "allow range for %s map '%s' must be a min and a max" % (name, map))
            map_list.append(BldCfgOptionAllowRangeMap(map, range[0], range[1]))

    return BldCfgOptionAllowRange(map_list=map_list)


####################################################################################################################################
def _default(default_raw):
    """Parse a default, which is either the value or a value per value of the option it maps on.

    A tilde overrides an inherited default back to no default at all."""

    # A scalar is the default itself
    if isinstance(default_raw, str):
        return None if default_raw == "~" else BldCfgOptionDefault(value=default_raw)

    map_list = []

    for entry in default_raw:
        for map, value in entry:
            map_list.append(BldCfgOptionDefaultMap(map, value))

    return BldCfgOptionDefault(map_list=map_list)


####################################################################################################################################
def _depend(depend_raw, opt_list):
    """Parse a dependency, which is either the dependency or the name of an option to take it from."""

    # A scalar names the option the dependency is inherited from
    if isinstance(depend_raw, str):
        opt_inherit = _find(opt_list, depend_raw)

        if opt_inherit is None:
            raise ToolError("dependency inherited from option '%s' before it is defined" % depend_raw)

        return opt_inherit.depend

    option = None
    default_value = None
    value_list = None

    for key, value in depend_raw:
        if key == "list":
            value_list = list(value)
        elif key == "default":
            default_value = value
        elif key == "option":
            option = value
        else:
            raise ToolError("unknown depend definition '%s'" % key)

    return BldCfgOptionDepend(option, default_value, value_list)


####################################################################################################################################
def _depend_reconcile(opt, depend, opt_list):
    """Resolve a dependency to the option it is on, now that every option exists."""

    if depend is None:
        return None

    if depend.default_value is not None and opt.type not in (OPT_TYPE_BOOLEAN, OPT_TYPE_INTEGER):
        raise ToolError("dependency default invalid for non integer/boolean option '%s'" % opt.name)

    opt_depend = _find(opt_list, depend.option)

    if opt_depend is None:
        raise ToolError("dependency on undefined option '%s'" % depend.option)

    return BldCfgOptionDepend(opt_depend, depend.default_value, depend.value_list)


####################################################################################################################################
def _deprecate_list(deprecate_raw):
    """Parse the deprecated names of an option.

    A question mark in the name means the deprecation may be indexed, e.g. repo?-path covers repo-path and repo1-path, so the same
    name can appear twice and each spelling is recorded separately."""

    result = []

    for name, detail in deprecate_raw:
        yaml_map_empty(detail, "deprecate '%s'" % name)

        indexed = "?" in name
        name = name.replace("?", "", 1)

        deprecate = _find(result, name)

        if deprecate is None:
            deprecate = BldCfgOptionDeprecate(name)
            result.append(deprecate)

        if indexed:
            deprecate.indexed = True
        else:
            deprecate.unindexed = True

    return sorted(result, key=lambda deprecate: deprecate.name)


####################################################################################################################################
def _option_command_list(opt_cmd_raw, cmd_list, opt_list, name):
    """Parse the commands an option is valid for, which is either the list or the name of an option to take it from."""

    # A scalar names the option the command list is inherited from
    if isinstance(opt_cmd_raw, str):
        opt_inherit = _find(opt_list, opt_cmd_raw)
        check(opt_inherit is not None, "command list inherited from option '%s' before it is defined" % opt_cmd_raw)

        return opt_inherit.cmd_list

    result = []

    for opt_cmd, detail in opt_cmd_raw:
        # Add every command of another option, stripped of what that option declared about them
        if opt_cmd == "+inherit":
            opt_inherit = _find(opt_list, detail)
            check(opt_inherit is not None, "commands inherited from option '%s' before it is defined" % detail)

            for cmd in opt_inherit.cmd_list:
                result.append(BldCfgOptionCommand(cmd.name))
        # Add every command that has a role, or any role
        elif opt_cmd == "+role":
            for cmd in cmd_list:
                # Help and version take no options of their own
                if cmd.name in (CMD_HELP, CMD_VERSION):
                    continue

                for role in cmd.role_list:
                    if (detail == "any" or role == detail) and _find(result, cmd.name) is None:
                        result.append(BldCfgOptionCommand(cmd.name))
        # Remove a command, which is how a command is trimmed from what +inherit or +role added
        elif opt_cmd == "-command":
            cmd = _find(result, detail)

            if cmd is not None:
                result.remove(cmd)
        # Else the command is declared with what it overrides
        else:
            cmd = BldCfgOptionCommand(opt_cmd)

            for key, value in detail:
                if key == "allow-list":
                    cmd.allow_list = _allow_list(value, None, "%s command '%s'" % (name, opt_cmd))
                elif key == "command-role":
                    cmd.role_list = _command_role(value, "%s command '%s'" % (name, opt_cmd))
                elif key == "depend":
                    cmd.depend = _depend(value, opt_list)
                elif key == "default":
                    cmd.default_value = _default(value)
                elif key == "internal":
                    cmd.internal = yaml_bool(value, "%s command '%s' internal" % (name, opt_cmd))
                elif key == "required":
                    cmd.required = yaml_bool(value, "%s command '%s' required" % (name, opt_cmd))
                elif key == "sequence":
                    cmd.sequence = yaml_bool(value, "%s command '%s' sequence" % (name, opt_cmd))
                else:
                    raise ToolError("unknown option command definition '%s'" % key)

            # A declaration replaces whatever +inherit or +role added for the command
            cmd_prior = _find(result, opt_cmd)

            if cmd_prior is not None:
                result.remove(cmd_prior)

            result.append(cmd)

    return sorted(result, key=lambda cmd: cmd.name)


####################################################################################################################################
def _option_list_raw(opt_raw, cmd_list, opt_grp_list):
    """Parse the options as declared, before any of them are resolved against each other."""

    result = []

    for name, detail in opt_raw:
        opt = BldCfgOption(name)
        inherit_found = False

        for key, value in detail:
            if key == "allow-list":
                opt.allow_list = _allow_list(value, result, "option '%s'" % name)
            elif key == "allow-range":
                opt.allow_range = _allow_range(value, "option '%s'" % name)
            elif key == "command":
                opt.cmd_list = _option_command_list(value, cmd_list, result, "option '%s'" % name)
            elif key == "command-role":
                opt.cmd_role_list = _command_role(value, "option '%s'" % name)
            elif key == "default":
                opt.default_value = _default(value)
            elif key == "depend":
                opt.depend = _depend(value, result)
            elif key == "deprecate":
                opt.deprecate_list = _deprecate_list(value)
            elif key == "default-type":
                check(value in _DEFAULT_TYPE_LIST, "option '%s' has invalid default type '%s'" % (name, value))
                opt.default_type = value
            elif key == "group":
                check(_find(opt_grp_list, value) is not None, "option '%s' has invalid group '%s'" % (name, value))
                opt.group = value
            elif key == "inherit":
                opt_inherit = _find(result, value)
                check(opt_inherit is not None, "option '%s' inherited before it is defined" % value)

                # Everything declared before inherit is replaced and everything after it overrides, which is why inherit is written
                # first. Deprecations are not inherited since they name the option they are a deprecation of.
                opt = copy.copy(opt_inherit)
                opt.name = name
                opt.deprecate_list = None

                inherit_found = True
            elif key == "internal":
                opt.internal = yaml_bool(value, "option '%s' internal" % name)
            elif key == "bool-like":
                opt.bool_like = yaml_bool(value, "option '%s' bool-like" % name)
            elif key == "beta":
                opt.beta = yaml_bool(value, "option '%s' beta" % name)
            elif key == "negate":
                opt.negate = yaml_bool(value, "option '%s' negate" % name)
            elif key == "sequence":
                opt.sequence = yaml_bool(value, "option '%s' sequence" % name)
            elif key == "required":
                opt.required = yaml_bool(value, "option '%s' required" % name)
            elif key == "section":
                opt.section = value
            elif key == "secure":
                opt.secure = yaml_bool(value, "option '%s' secure" % name)
            elif key == "type":
                opt.type = value
            else:
                raise ToolError("unknown option definition '%s'" % key)

        if opt.type is None:
            raise ToolError("option '%s' requires 'type'" % name)

        # An inherited option already has these set from the option it inherited from
        if not inherit_found:
            # An option that does not say where it can be configured can only be given on the command line
            if opt.section is None:
                opt.section = SECTION_COMMAND_LINE

            # Only an option that can be written in the configuration file can be negated there
            if opt.negate is None:
                opt.negate = (opt.type == OPT_TYPE_BOOLEAN or opt.bool_like) and opt.section != SECTION_COMMAND_LINE

            # An option that does not name its commands is valid for all of them
            if opt.cmd_list is None:
                opt.cmd_list = sorted(
                    (BldCfgOptionCommand(cmd.name) for cmd in cmd_list if cmd.name not in (CMD_HELP, CMD_VERSION)),
                    key=lambda cmd: cmd.name,
                )

        # Only an option that can be written in the configuration file can be reset, which is what removes it again
        opt.reset = opt.section != SECTION_COMMAND_LINE

        result.append(opt)

    return sorted(result, key=lambda opt: opt.name)


####################################################################################################################################
def _option_list(opt_raw, cmd_list, opt_grp_list):
    """Parse the option list and resolve every option against the others."""

    opt_list_raw = _option_list_raw(opt_raw, cmd_list, opt_grp_list)

    # Copy the options so a dependency can point at the option it is on rather than at the name of one
    result = []

    for opt_raw in opt_list_raw:
        opt = copy.copy(opt_raw)
        opt.cmd_list = None
        opt.depend = None
        result.append(opt)

    # Resolve what each option means for each of its commands, which is what it declares for the command falling back to what it
    # declares for itself
    for idx, opt_raw in enumerate(opt_list_raw):
        cmd_opt_list = []

        for opt_cmd_raw in opt_raw.cmd_list:
            cmd = _find(cmd_list, opt_cmd_raw.name)

            if cmd is None:
                raise ToolError("invalid command '%s' in option '%s' command list" % (opt_cmd_raw.name, opt_raw.name))

            opt_cmd = copy.copy(opt_cmd_raw)

            if opt_cmd.required is None:
                opt_cmd.required = opt_raw.required

            if opt_cmd.internal is None:
                opt_cmd.internal = opt_raw.internal

            if opt_cmd.sequence is None:
                opt_cmd.sequence = opt_raw.sequence

            if opt_cmd.role_list is None:
                # What the option declares for every command, less the roles this command does not have
                if opt_raw.cmd_role_list is not None:
                    opt_cmd.role_list = [role for role in opt_raw.cmd_role_list if role in cmd.role_list]
                else:
                    opt_cmd.role_list = cmd.role_list

            opt_cmd.depend = _depend_reconcile(opt_raw, opt_cmd_raw.depend, result)
            cmd_opt_list.append(opt_cmd)

        result[idx].cmd_list = cmd_opt_list
        result[idx].depend = _depend_reconcile(opt_raw, opt_raw.depend, result)

    return result


####################################################################################################################################
def _option_resolve_list(opt_list):
    """Order the options so that every option comes after the options it depends on.

    The stanza option is resolved first since an error about a missing option is confusing when the stanza it would apply to is what is
    actually missing, so stanza must exist and may not depend on anything."""

    opt_stanza = _find(opt_list, OPT_STANZA)

    if opt_stanza is None:
        raise ToolError("option '%s' must exist" % OPT_STANZA)

    if opt_stanza.depend is not None:
        raise ToolError("option '%s' may not depend on other option" % OPT_STANZA)

    for opt_cmd in opt_stanza.cmd_list:
        if opt_cmd.depend is not None:
            raise ToolError("option '%s' command '%s' may not depend on other option" % (OPT_STANZA, opt_cmd.name))

    resolve_list = [OPT_STANZA]

    while len(resolve_list) != len(opt_list):
        resolved = False

        for opt in opt_list:
            if opt.name in resolve_list:
                continue

            if opt.depend is not None and opt.depend.option.name not in resolve_list:
                continue

            if any(cmd.depend is not None and cmd.depend.option.name not in resolve_list for cmd in opt.cmd_list):
                continue

            resolve_list.append(opt.name)
            resolved = True

        # Nothing resolved means the options left depend on each other, since every other reason to skip is already excluded
        if not resolved:
            raise ToolError(
                "unable to resolve dependencies for option(s) '%s'\nHINT: are there circular dependencies?"
                % ", ".join(opt.name for opt in opt_list if opt.name not in resolve_list)
            )

    return [_find(opt_list, name) for name in resolve_list]


####################################################################################################################################
def bld_cfg_parse(path_repo):
    """Parse config.yaml into the configuration declaration."""

    path = os.path.join(path_repo, "build/config.yaml")
    cfg_raw = yaml_load(file_read(path), path)

    cmd_raw = None
    opt_grp_raw = None
    opt_raw = None

    for key, value in cfg_raw:
        if key == "command":
            cmd_raw = value
        elif key == "optionGroup":
            opt_grp_raw = value
        elif key == "option":
            opt_raw = value
        else:
            raise ToolError("unknown config definition '%s'" % key)

    check(cmd_raw is not None and opt_grp_raw is not None and opt_raw is not None, "command, optionGroup, and option are required")

    cmd_list = _command_list(cmd_raw)
    opt_grp_list = _option_group_list(opt_grp_raw)
    opt_list = _option_list(opt_raw, cmd_list, opt_grp_list)

    return BldCfg(cmd_list, opt_grp_list, opt_list, _option_resolve_list(opt_list))
