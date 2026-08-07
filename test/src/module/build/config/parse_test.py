"""Test Configuration Parse.

The declaration is written out here rather than read from the repository, since what needs testing is what a declaration means --
and in particular what it inherits -- rather than what the project happens to declare today.

Most tests share one declaration and check a different part of the result, so the parts that inherit from each other stay in one
place and it is clear what each option was declared against."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_write
from config.parse import *

# Commands the options are declared against. Backup sets everything a command can set, help and version take no options of their
# own, and restore takes only what a command gets by default.
COMMAND = """command:
  backup:
    command-role:
      async: {}
      local: {}
      remote: {}
    internal: true
    lock-type: backup
    lock-required: true
    lock-remote-required: true
    log-file: false
    log-level-default: DETAIL
    parameter-allowed: true

  help: {}

  restore: {}

  version: {}

optionGroup:
  pg: {}
  repo: {}

option:
"""

# An option that everything else can be declared against
OPTION_STANZA = """  stanza:
    type: string
"""


####################################################################################################################################
def _parse(option, command=COMMAND):
    """Parse a declaration built from the option section given."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), command + option)

        return bld_cfg_parse(path)


####################################################################################################################################
def _error(option, command=COMMAND):
    """Parse a declaration that is expected to fail and return the message."""

    with assert_raises(ToolError) as error:
        _parse(option, command)

    return str(error.exception)


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name."""

    return next(item for item in item_list if item.name == name)


####################################################################################################################################
def test_config_parse_command():
    """A command takes what it declares, and every command has a main role whether it declares one or not."""

    cfg = _parse(OPTION_STANZA)

    # Commands are sorted, whatever order they were declared in
    assert_equal([cmd.name for cmd in cfg.cmd_list], ["backup", "help", "restore", "version"])
    assert_equal([opt_grp.name for opt_grp in cfg.opt_grp_list], ["pg", "repo"])

    cmd = _find(cfg.cmd_list, "backup")

    assert_true(cmd.internal)
    assert_equal(cmd.lock_type, "backup")
    assert_true(cmd.lock_required)
    assert_true(cmd.lock_remote_required)
    assert_false(cmd.log_file)
    assert_equal(cmd.log_level_default, "detail")
    assert_true(cmd.parameter_allowed)

    # Main is added to the roles and the result is sorted
    assert_equal(cmd.role_list, ["async", "local", "main", "remote"])

    # A command that declares nothing gets the defaults, which includes writing to a log file
    cmd = _find(cfg.cmd_list, "restore")

    assert_false(cmd.internal)
    assert_equal(cmd.lock_type, "none")
    assert_true(cmd.log_file)
    assert_equal(cmd.log_level_default, "info")
    assert_equal(cmd.role_list, ["main"])


####################################################################################################################################
def test_config_parse_command_error():
    """A command definition that is not one is reported rather than ignored."""

    assert_equal(
        _error(OPTION_STANZA, "command:\n  backup:\n    bogus: true\n\noptionGroup:\n  pg: {}\n\noption:\n"),
        "unknown command definition 'bogus'",
    )

    assert_equal(
        _error(OPTION_STANZA, "command:\n  backup:\n    command-role:\n      main: value\n\noptionGroup:\n  pg: {}\n\noption:\n"),
        "command 'backup' role 'main' must be an empty map",
    )

    assert_equal(
        _error(OPTION_STANZA, "command:\n  backup:\n    internal: yes\n\noptionGroup:\n  pg: {}\n\noption:\n"),
        "invalid boolean 'yes' for command 'backup' internal",
    )

    assert_equal(
        _error(OPTION_STANZA, "command:\n  backup: {}\n\noptionGroup:\n  pg: value\n\noption:\n"),
        "option group 'pg' must be an empty map",
    )


####################################################################################################################################
def test_config_parse_option():
    """An option takes what it declares, and what it does not declare follows from its type and section."""

    cfg = _parse(
        OPTION_STANZA
        + """  buffer-size:
    section: global
    type: size
    default: 1MiB
    allow-range: [16KiB, 1GiB]
    beta: true
    secure: true

  force:
    type: boolean

  online:
    section: global
    type: boolean
    default: true
    negate: false

  repo-cipher-type:
    section: global
    type: string-id
    sequence: true

  start-fast:
    section: global
    type: boolean
    bool-like: true
    default-type: literal

  pg-path:
    section: global
    group: pg
    type: path
    required: false
    deprecate:
      db-path: {}
      db?-path: {}
"""
    )

    # Options are sorted, whatever order they were declared in
    assert_equal(
        [opt.name for opt in cfg.opt_list],
        ["buffer-size", "force", "online", "pg-path", "repo-cipher-type", "stanza", "start-fast"],
    )

    opt = _find(cfg.opt_list, "buffer-size")

    assert_equal(opt.type, "size")
    assert_equal(opt.section, "global")
    assert_equal(opt.default_value.value, "1MiB")
    assert_equal(opt.default_type, "quote")
    assert_equal((opt.allow_range.min, opt.allow_range.max), ("16KiB", "1GiB"))
    assert_true(opt.beta)
    assert_true(opt.secure)

    # An option in the configuration file can be reset, which is what removes it again
    assert_true(opt.reset)

    # Only a boolean in the configuration file can be negated, unless the option says otherwise
    assert_false(opt.negate)
    assert_false(_find(cfg.opt_list, "force").negate)
    assert_false(_find(cfg.opt_list, "online").negate)

    # An option that accepts y/n is treated as a boolean, so it can be negated too
    opt = _find(cfg.opt_list, "start-fast")

    assert_true(opt.bool_like)
    assert_true(opt.negate)
    assert_equal(opt.default_type, "literal")

    # An option whose values are numbered
    assert_true(_find(cfg.opt_list, "repo-cipher-type").sequence)

    # An option that does not say where it can be configured can only be given on the command line, so it cannot be reset
    opt = _find(cfg.opt_list, "force")

    assert_equal(opt.section, "command-line")
    assert_false(opt.reset)
    assert_true(opt.required)

    # A command list that is not declared is every command but the ones that take no options
    assert_equal([cmd.name for cmd in opt.cmd_list], ["backup", "restore"])

    opt = _find(cfg.opt_list, "pg-path")

    assert_equal(opt.group, "pg")
    assert_false(opt.required)

    # The same deprecated name written both ways is one deprecation that can be spelled either way
    assert_equal([(dep.name, dep.indexed, dep.unindexed) for dep in opt.deprecate_list], [("db-path", True, True)])


####################################################################################################################################
def test_config_parse_option_error():
    """An option declaration that cannot be honored is reported rather than ignored."""

    assert_equal(_error(OPTION_STANZA + "  bogus:\n    bogus: true\n"), "unknown option definition 'bogus'")
    assert_equal(_error(OPTION_STANZA + "  bogus:\n    section: global\n"), "option 'bogus' requires 'type'")
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    default-type: bogus\n"),
        "option 'bogus' has invalid default type 'bogus'",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    group: bogus\n"), "option 'bogus' has invalid group 'bogus'"
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    command:\n      bogus: {}\n"),
        "invalid command 'bogus' in option 'bogus' command list",
    )

    # The stanza option must exist and must not depend on anything, since it is resolved before everything else
    assert_equal(_error("  force:\n    type: boolean\n"), "option 'stanza' must exist")
    assert_equal(
        _error("  force:\n    type: boolean\n  stanza:\n    type: string\n    depend:\n      option: force\n"),
        "option 'stanza' may not depend on other option",
    )
    assert_equal(
        _error(
            "  force:\n    type: boolean\n"
            "  stanza:\n    type: string\n    command:\n      backup:\n        depend:\n          option: force\n"
        ),
        "option 'stanza' command 'backup' may not depend on other option",
    )


####################################################################################################################################
def test_config_parse_inherit():
    """An option can be declared against another option, which is what keeps a family of options in step."""

    cfg = _parse(
        OPTION_STANZA
        + """  repo-path:
    section: global
    group: repo
    type: path
    default: /var/lib/pgbackrest
    command:
      backup: {}
    deprecate:
      repo-path: {}

  repo-host:
    inherit: repo-path
    type: string
    default: ~

  repo-host-user:
    inherit: repo-host
    default: pgbackrest

  repo-retention-full:
    section: global
    group: repo
    type: integer
    allow-list:
      - 1
      - 2
    command: repo-path
"""
    )

    # Everything is inherited but what is declared after the inherit, and a tilde takes the default away again
    opt = _find(cfg.opt_list, "repo-host")

    assert_equal(opt.type, "string")
    assert_equal(opt.section, "global")
    assert_equal(opt.group, "repo")
    assert_is_none(opt.default_value)
    assert_equal([cmd.name for cmd in opt.cmd_list], ["backup"])

    # A deprecation names the option it is a deprecation of, so it is not inherited
    assert_is_none(opt.deprecate_list)
    assert_equal([dep.name for dep in _find(cfg.opt_list, "repo-path").deprecate_list], ["repo-path"])

    # Inheriting from an option that itself inherited
    opt = _find(cfg.opt_list, "repo-host-user")

    assert_equal(opt.type, "string")
    assert_equal(opt.default_value.value, "pgbackrest")

    # A command list can be taken from another option on its own
    assert_equal([cmd.name for cmd in _find(cfg.opt_list, "repo-retention-full").cmd_list], ["backup"])


####################################################################################################################################
def test_config_parse_inherit_error():
    """An option can only be declared against an option above it, since that is the order the declaration is read in."""

    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    inherit: later\n  later:\n    type: string\n"),
        "option 'later' inherited before it is defined",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    command: later\n  later:\n    type: string\n"),
        "command list inherited from option 'later' before it is defined",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    command:\n      +inherit: later\n  later:\n    type: string\n"),
        "commands inherited from option 'later' before it is defined",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    allow-list: later\n  later:\n    type: string\n"),
        "allow list inherited from option 'later' before it is defined",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    depend: later\n  later:\n    type: string\n"),
        "dependency inherited from option 'later' before it is defined",
    )


####################################################################################################################################
def test_config_parse_allow():
    """The values an option allows can be listed, ranged, or taken from another option."""

    cfg = _parse(
        OPTION_STANZA
        + """  repo-type:
    section: global
    type: string-id
    default: posix
    allow-list:
      - posix
      - s3
      - sftp: HAVE_LIBSSH2

  repo-type-other:
    section: global
    type: string-id
    allow-list: repo-type

  process-max:
    section: global
    type: integer
    allow-range: [1, 999]

  compress-level:
    section: global
    type: integer
    allow-range:
      - none: [0, 0]
      - gz: [0, 9]
"""
    )

    opt = _find(cfg.opt_list, "repo-type")

    assert_equal(
        [(allow.value, allow.condition) for allow in opt.allow_list], [("posix", None), ("s3", None), ("sftp", "HAVE_LIBSSH2")]
    )

    # An allow list taken from another option is the same list
    assert_equal([allow.value for allow in _find(cfg.opt_list, "repo-type-other").allow_list], ["posix", "s3", "sftp"])

    # A range that maps on the values of another option
    allow_range = _find(cfg.opt_list, "compress-level").allow_range

    assert_is_none(allow_range.min)
    assert_equal([(map.map, map.min, map.max) for map in allow_range.map_list], [("none", "0", "0"), ("gz", "0", "9")])


####################################################################################################################################
def test_config_parse_allow_error():
    """A range or a conditional value that is not written as one is reported."""

    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: integer\n    allow-range: [1, 2, 3]\n"),
        "allow range for option 'bogus' must be a min and a max",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: integer\n    allow-range:\n      - none: [0]\n"),
        "allow range for option 'bogus' map 'none' must be a min and a max",
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string-id\n    allow-list:\n      - a: X\n        b: Y\n"),
        "allow list value for option 'bogus' must have a single condition",
    )

    # An allow list cannot be inherited inside a command, where there is no option list to inherit from
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string-id\n    command:\n      backup:\n        allow-list: stanza\n"),
        "allow list for option 'bogus' command 'backup' cannot be inherited",
    )


####################################################################################################################################
def test_config_parse_depend():
    """A dependency is on another option and on the values of it that make this option valid."""

    cfg = _parse(
        OPTION_STANZA
        + """  online:
    section: global
    type: boolean
    default: true

  archive-check:
    section: global
    type: boolean
    default: true
    depend:
      option: online
      default: false
      list:
        - true

  archive-copy:
    section: global
    type: boolean
    depend: archive-check
"""
    )

    depend = _find(cfg.opt_list, "archive-check").depend

    # The dependency points at the option itself rather than at its name, so the render can reach what it needs
    assert_equal(depend.option.name, "online")
    assert_equal(depend.option.type, "boolean")
    assert_equal(depend.default_value, "false")
    assert_equal(depend.value_list, ["true"])

    # A dependency taken from another option is the same dependency
    assert_equal(_find(cfg.opt_list, "archive-copy").depend.option.name, "online")


####################################################################################################################################
def test_config_parse_depend_error():
    """A dependency that cannot be honored is reported."""

    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    depend:\n      bogus: true\n"), "unknown depend definition 'bogus'"
    )
    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    depend:\n      option: missing\n"),
        "dependency on undefined option 'missing'",
    )

    # Only an option the C can default without knowing the value can have a dependency default
    assert_equal(
        _error(
            OPTION_STANZA
            + "  online:\n    type: boolean\n"
            + "  bogus:\n    type: string\n    depend:\n      option: online\n      default: x\n"
        ),
        "dependency default invalid for non integer/boolean option 'bogus'",
    )


####################################################################################################################################
def test_config_parse_option_command():
    """What an option means for a command is what it declares for the command falling back to what it declares for itself."""

    cfg = _parse(
        OPTION_STANZA
        + """  set:
    type: string
    internal: true
    required: false
    command:
      +role: any
      -command: restore
      -command: missing
      backup:
        internal: false
        required: true
        sequence: true
        default: latest
        allow-list:
          - latest
        command-role:
          main: {}
          local: {}

  type:
    type: string
    command:
      +inherit: set
      +role: local
"""
    )

    opt = _find(cfg.opt_list, "set")

    # Every command that has any role, less the ones removed, and never the commands that take no options
    assert_equal([cmd.name for cmd in opt.cmd_list], ["backup"])

    cmd = _find(opt.cmd_list, "backup")

    # What the command declares wins over what the option declares
    assert_false(cmd.internal)
    assert_true(cmd.required)
    assert_true(cmd.sequence)
    assert_equal(cmd.default_value.value, "latest")
    assert_equal([allow.value for allow in cmd.allow_list], ["latest"])

    # A role list declared for the command is the command's own
    assert_equal(cmd.role_list, ["main", "local"])

    # A command that takes its own roles from the option keeps only the roles that command has
    opt = _find(cfg.opt_list, "type")

    assert_equal([cmd.name for cmd in opt.cmd_list], ["backup"])
    assert_equal(_find(opt.cmd_list, "backup").role_list, ["async", "local", "main", "remote"])

    # What the command does not declare follows the option
    cmd = _find(opt.cmd_list, "backup")

    assert_false(cmd.internal)
    assert_true(cmd.required)
    assert_false(cmd.sequence)


####################################################################################################################################
def test_config_parse_option_command_role():
    """An option can name the roles it is valid for, which are then filtered against the roles each command has."""

    cfg = _parse(
        OPTION_STANZA
        + """  process:
    type: integer
    command-role:
      local: {}
      remote: {}
    command:
      backup: {}
      restore: {}
"""
    )

    opt = _find(cfg.opt_list, "process")

    # Backup has both roles and restore has neither, so restore is left with nothing
    assert_equal(_find(opt.cmd_list, "backup").role_list, ["local", "remote"])
    assert_equal(_find(opt.cmd_list, "restore").role_list, [])


####################################################################################################################################
def test_config_parse_option_command_error():
    """A command definition inside an option that is not one is reported."""

    assert_equal(
        _error(OPTION_STANZA + "  bogus:\n    type: string\n    command:\n      backup:\n        bogus: true\n"),
        "unknown option command definition 'bogus'",
    )


####################################################################################################################################
def test_config_parse_default():
    """A default can be one value or a value per value of the option it maps on."""

    cfg = _parse(
        OPTION_STANZA
        + """  repo-type:
    section: global
    type: string-id
    default: posix

  repo-path:
    section: global
    type: path
    default:
      - posix: /var/lib/pgbackrest
      - s3: /
"""
    )

    default = _find(cfg.opt_list, "repo-path").default_value

    assert_is_none(default.value)
    assert_equal([(map.map, map.value) for map in default.map_list], [("posix", "/var/lib/pgbackrest"), ("s3", "/")])


####################################################################################################################################
def test_config_parse_resolve():
    """The options are ordered so that every option comes after the options it depends on."""

    cfg = _parse(
        OPTION_STANZA
        + """  online:
    section: global
    type: boolean

  archive-check:
    section: global
    type: boolean
    depend:
      option: online

  archive-copy:
    section: global
    type: boolean
    command:
      backup:
        depend:
          option: archive-check
"""
    )

    # Stanza is always first, since an error about a missing option is confusing when the stanza is what is missing. After that an
    # option follows whatever it depends on, whether the dependency is its own or one of its commands.
    assert_equal([opt.name for opt in cfg.opt_resolve_list], ["stanza", "online", "archive-check", "archive-copy"])


####################################################################################################################################
def test_config_parse_resolve_error():
    """Options that depend on each other cannot be ordered and are reported together."""

    assert_equal(
        _error(
            OPTION_STANZA
            + "  a:\n    type: boolean\n    depend:\n      option: b\n"
            + "  b:\n    type: boolean\n    depend:\n      option: a\n"
        ),
        "unable to resolve dependencies for option(s) 'a, b'\nHINT: are there circular dependencies?",
    )


####################################################################################################################################
def test_config_parse_error():
    """A declaration that is missing a section, or has one that is not expected, is reported."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), "bogus:\n  key: value\n")

        with assert_raises(ToolError) as error:
            bld_cfg_parse(path)

        assert_equal(str(error.exception), "unknown config definition 'bogus'")

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), "command:\n  backup: {}\n")

        with assert_raises(ToolError) as error:
            bld_cfg_parse(path)

        assert_equal(str(error.exception), "command, optionGroup, and option are required")
