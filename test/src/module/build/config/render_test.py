"""Test Configuration Render.

The declaration written here is a small one that still has an option of every type, since the value tables the rules index into are
built per type and a type with no values in it would not be rendered at all.

The rules are checked as the fragments that matter rather than as whole files, since the whole of the real parse rules is a hundred
thousand lines. What guarantees the rest is that the same generator reproduces the checked-in files byte for byte."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from common.error import *
from common.storage import file_read, file_write
from config.parse import bld_cfg_parse
from config.render import *

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
    log-level-default: detail
    parameter-allowed: true

  help: {}

  restore: {}

  version: {}

optionGroup:
  pg: {}
  repo: {}

option:
"""

OPTION = """  annotation:
    section: global
    type: hash

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
    default: false
    command:
      backup:
        default: true
        depend:
          option: archive-check

  buffer-size:
    section: global
    type: size
    default: 1MiB
    allow-range: [16KiB, 1GiB]

  compress-level:
    section: global
    type: integer
    default-type: dynamic
    default: compress-level
    allow-range:
      - none: [0, 0]
      - gz: [0, 9]

  db-include:
    section: global
    type: list

  force:
    type: boolean
    internal: true
    beta: true
    secure: true
    command:
      backup:
        internal: false
        required: false
      restore:
        internal: false
        required: false

  io-timeout:
    section: global
    type: time
    default: 60
    allow-range: [100ms, 1h]

  online:
    section: global
    type: boolean
    default: true

  pg-path:
    section: global
    group: pg
    type: path
    deprecate:
      pg-path: {}
      db?-path: {}

  process-max:
    section: global
    type: integer
    default: 1
    allow-range: [1, 999]

  repo-cipher-type:
    section: global
    group: repo
    type: string-id
    sequence: true
    default: aes-256-cbc
    allow-list:
      - none
      - aes-256-cbc

  repo-host:
    section: global
    group: repo
    type: string

  repo-host-user:
    section: global
    group: repo
    type: string
    default: pgbackrest
    depend:
      option: repo-host

  repo-path:
    section: global
    group: repo
    type: path
    default: /var/lib/pgbackrest

  repo-storage-port:
    section: global
    group: repo
    type: integer
    default:
      - s3: 443
      - azure: 443

  repo-type:
    section: global
    group: repo
    type: string-id
    default: posix
    allow-list:
      - posix
      - posix
      - s3
      - sftp: HAVE_LIBSSH2
    command:
      backup:
        allow-list:
          - posix
          - azure

  spool-path:
    section: global
    type: path
    default-type: literal
    default: PROJECT_PATH

  stanza:
    type: string

  start-fast:
    section: global
    type: boolean
    bool-like: true
    default: false

  target:
    type: string
    command:
      restore: {}

  target-action:
    section: global
    type: string-id
    sequence: true
    default: pause
    allow-list:
      - pause
      - promote

  repo-storage-host:
    section: global
    group: repo
    type: string
    default: this-is-a-very-long-default-value-that-leaves-no-room-for-a-label-on-the-line-it-is-rendered-on

  tls-version:
    section: global
    type: string
    default: any version
    allow-list:
      - "1.2"
      - "1.3"

  type:
    type: string-id
    default: incr
    allow-list:
      - full
      - diff
      - incr
    command:
      backup:
        required: false
      restore:
        sequence: true
        default: default
        allow-list:
          - default
          - immediate
"""


####################################################################################################################################
def _render(option=OPTION, label=False):
    """Render a declaration and return both generated files.

    Labels are off unless a test is checking them, since a label is appended to every line and would be in the way of reading the
    rules the rest of the tests check."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), COMMAND + option)
        bld_cfg_render(path, bld_cfg_parse(path), label)

        return (
            file_read(os.path.join(path, "src/config/config.auto.h")),
            file_read(os.path.join(path, "src/config/parse.auto.c.inc")),
        )


####################################################################################################################################
def test_config_render_constant():
    """A command, an option, and an option value each get the constant the code refers to it by."""

    config, _ = _render()

    assert_in('#define CFGCMD_BACKUP                                               "backup"\n', config)
    assert_in("#define CFG_COMMAND_TOTAL                                           4\n", config)
    assert_in("#define CFG_OPTION_GROUP_TOTAL                                      2\n", config)

    # An option in a group is referred to by the group and an index rather than by name, so it has no constant
    assert_in('#define CFGOPT_STANZA                                               "stanza"\n', config)
    assert_not_in("CFGOPT_REPO_PATH ", config)
    assert_in("#define CFG_OPTION_TOTAL                                            25\n", config)


####################################################################################################################################
def test_config_render_value_constant():
    """A value of a string-id option gets the StringId it encodes to and the string it spells."""

    config, _ = _render()

    assert_in('#define CFGOPTVAL_REPO_TYPE_POSIX_Z                                 "posix"\n', config)
    assert_in('#define CFGOPTVAL_REPO_TYPE_POSIX                                   STRID5("posix", 0x', config)

    # An option whose values are numbered also gets the number, and the StringId carries it
    assert_in("#define CFGOPTVAL_REPO_CIPHER_TYPE_NONE                             0\n", config)
    assert_in('#define CFGOPTVAL_REPO_CIPHER_TYPE_NONE_STRID                       STRID5S("none", 0, 0x', config)

    # Values an option allows only for one command are named after the command as well
    assert_in("#define CFGOPTVAL_RESTORE_TYPE_DEFAULT                              0\n", config)
    assert_in("#define CFGOPTVAL_RESTORE_TYPE_IMMEDIATE                            1\n", config)


####################################################################################################################################
def test_config_render_enum():
    """Every command, option group, and option is in the enum the rules index it by."""

    config, _ = _render()

    assert_in(
        """typedef enum
{
    cfgCmdBackup,
    cfgCmdHelp,
    cfgCmdRestore,
    cfgCmdVersion,
} ConfigCommand;
""",
        config,
    )

    assert_in(
        """typedef enum
{
    cfgOptGrpPg,
    cfgOptGrpRepo,
} ConfigOptionGroup;
""",
        config,
    )

    assert_in("    cfgOptRepoCipherType,\n", config)


####################################################################################################################################
def test_config_render_command():
    """A command renders what it declares, and only what differs from the default."""

    _, parse = _render()

    assert_in(
        """    PARSE_RULE_COMMAND
    (
        PARSE_RULE_COMMAND_NAME("backup"),""",
        parse,
    )

    for rule in (
        "PARSE_RULE_COMMAND_INTERNAL(true)",
        "PARSE_RULE_COMMAND_LOCK_REQUIRED(true),",
        "PARSE_RULE_COMMAND_LOCK_REMOTE_REQUIRED(true),",
        "PARSE_RULE_COMMAND_LOCK_TYPE(Backup),",
        "PARSE_RULE_COMMAND_LOG_LEVEL_DEFAULT(Detail),",
        "PARSE_RULE_COMMAND_PARAMETER_ALLOWED(true),",
        "PARSE_RULE_COMMAND_ROLE(Async)",
    ):
        assert_in(rule, parse)

    # A command that writes to a log file says so, and backup does not
    assert_in("PARSE_RULE_COMMAND_LOG_FILE(true),", parse)

    # An option group is rendered by name, since the code refers to a group by its enum
    assert_in('        PARSE_RULE_OPTION_GROUP_NAME("pg"),', parse)


####################################################################################################################################
def test_config_render_option():
    """An option renders what it declares, and only what differs from the default."""

    _, parse = _render()

    for rule in (
        'PARSE_RULE_OPTION_NAME("force"),',
        "PARSE_RULE_OPTION_TYPE(Boolean),",
        "PARSE_RULE_OPTION_INTERNAL(true)",
        "PARSE_RULE_OPTION_BETA(true),",
        "PARSE_RULE_OPTION_SECURE(true),",
        "PARSE_RULE_OPTION_REQUIRED(true),",
        "PARSE_RULE_OPTION_SECTION(CommandLine),",
        "PARSE_RULE_OPTION_SEQUENCE(true),",
        "PARSE_RULE_OPTION_NEGATE(true),",
        "PARSE_RULE_OPTION_RESET(true),",
        "PARSE_RULE_OPTION_GROUP_ID(Repo),",
        "PARSE_RULE_OPTION_DEFAULT_TYPE(Dynamic),",
    ):
        assert_in(rule, parse)

    # A hash or a list can be given more than once, which builds the value up rather than replacing it
    assert_in("PARSE_RULE_OPTION_MULTI(true),", parse)

    # An option that accepts y/n is treated as a boolean
    assert_in("PARSE_RULE_OPTION_BOOL_LIKE(true),", parse)

    # An option in a group whose deprecated name is the option name can be given without an index
    assert_in("PARSE_RULE_OPTION_DEPRECATE_MATCH(true),", parse)

    # Commands the option is internal for, when that differs from the option itself
    assert_in(
        """        PARSE_RULE_OPTION_COMMAND_INTERNAL_LIST
        (
            PARSE_RULE_OPTION_COMMAND_INTERNAL(Backup, false),
            PARSE_RULE_OPTION_COMMAND_INTERNAL(Restore, false),
        )""",
        parse,
    )

    # Commands the option is valid for, per role
    assert_in("PARSE_RULE_OPTION_COMMAND_ROLE_MAIN_VALID_LIST", parse)
    assert_in("PARSE_RULE_OPTION_COMMAND_ROLE_ASYNC_VALID_LIST", parse)


####################################################################################################################################
def test_config_render_optional():
    """A rule that applies only to some commands is rendered with a filter naming them."""

    _, parse = _render()

    # Two commands whose rules came out the same share one group
    assert_in(
        """            PARSE_RULE_OPTIONAL_GROUP
            (
                PARSE_RULE_FILTER_CMD
                (
                    PARSE_RULE_VAL_CMD(Backup),
                    PARSE_RULE_VAL_CMD(Restore),
                ),

                PARSE_RULE_OPTIONAL_NOT_REQUIRED(),
            ),""",
        parse,
    )

    # A dependency names the option it is on and the values of it that make this option valid
    assert_in(
        """                PARSE_RULE_OPTIONAL_DEPEND
                (
                    PARSE_RULE_OPTIONAL_DEPEND_DEFAULT(PARSE_RULE_VAL_BOOL_FALSE),
                    PARSE_RULE_VAL_OPT(Online),
                    PARSE_RULE_VAL_BOOL_TRUE,
                )""",
        parse,
    )

    # A dependency on a string option is compared as a StringId, since it is on one of a small set of known values
    assert_in("PARSE_RULE_VAL_OPT(RepoHost),", parse)

    # A range, either one range or a range per value of the option it maps on
    assert_in(
        """                PARSE_RULE_OPTIONAL_ALLOW_RANGE
                (
                    PARSE_RULE_VAL_INT(1),
                    PARSE_RULE_VAL_INT(999),
                )""",
        parse,
    )

    assert_in(
        """                    PARSE_RULE_OPTIONAL_ALLOW_RANGE_MAP
                    (
                        PARSE_RULE_VAL_STRID(none),
                        PARSE_RULE_VAL_INT(0),
                        PARSE_RULE_VAL_INT(0),
                        PARSE_RULE_VAL_STRID(gz),
                        PARSE_RULE_VAL_INT(0),
                        PARSE_RULE_VAL_INT(9),
                    ),""",
        parse,
    )

    # A value only compiled in with a feature is replaced by false when the feature is not, so it keeps its index
    assert_in(
        """                    PARSE_RULE_VAL_STRID(sftp),
#ifndef HAVE_LIBSSH2
                        PARSE_RULE_BOOL_FALSE,
#endif""",
        parse,
    )

    # A default that is worked out at run time names what works it out rather than a value
    assert_in("PARSE_RULE_DEFAULT_DYNAMIC(CompressLevel),", parse)

    # A default per value of the option it maps on
    assert_in(
        """                    PARSE_RULE_OPTIONAL_DEFAULT_MAP
                    (
                        PARSE_RULE_VAL_STRID(s3),
                        PARSE_RULE_VAL_INT(443),
                        PARSE_RULE_VAL_STRID(azure),
                        PARSE_RULE_VAL_INT(443),
                    ),""",
        parse,
    )

    # A default that is not the first value the option allows also says where it sits, since that is what a sequence counts. A
    # default that is the first needs nothing, since that is where a sequence starts.
    assert_in("PARSE_RULE_VAL_SEQ(1),", parse)
    assert_not_in("PARSE_RULE_VAL_SEQ(0),", parse)

    # A command that numbers the values differently from the option
    assert_in("PARSE_RULE_OPTIONAL_SEQUENCE()", parse)


####################################################################################################################################
def test_config_render_value():
    """Every value the rules refer to is in the table for its type, sorted by what it means."""

    _, parse = _render()

    # A string is stored once and referred to by index, and a StringId is the string it spells
    assert_in("#define PARSE_RULE_VAL_STR(value)", parse)
    assert_in("#define PARSE_RULE_VAL_STRID(value)                                 PARSE_RULE_VAL_STR(QT_##value##_QT)\n", parse)

    # A value that holds characters a name cannot has them spelled out
    assert_in("parseRuleValStrQT_FS_var_FS_lib_FS_pgbackrest_QT,", parse)
    assert_in("parseRuleValStrQT_1_DT_2_QT,", parse)
    assert_in("parseRuleValStrQT_any_SP_version_QT,", parse)

    # A default written as the literal C to use is not quoted
    assert_in("parseRuleValStrPROJECT_PATH,", parse)

    # Sizes and times are sorted by what they mean rather than by how they are written, and each keeps the text it was written as
    assert_in(
        """static const int64_t parseRuleValueSize[] =
{
    16384,""",
        parse,
    )
    assert_in("    parseRuleValStrQT_16KiB_QT,", parse)
    assert_in(
        """static const unsigned int parseRuleValueTime[] =
{
    100,""",
        parse,
    )

    # Integers are sorted numerically and named after the value
    assert_in("    parseRuleValInt999,", parse)

    # The order the options are resolved in, which is what the parse follows
    assert_in("static const uint8_t optionResolveOrder[] =\n{\n    cfgOptStanza,", parse)

    # Deprecations, which are the old names an option can still be given by
    assert_in("#define CFG_OPTION_DEPRECATE_TOTAL                                  2\n", parse)
    assert_in(
        """        .name = "db-path",
        .id = cfgOptPgPath,
        .indexed = true,""",
        parse,
    )
    assert_in(
        """        .name = "pg-path",
        .id = cfgOptPgPath,
        .unindexed = true,""",
        parse,
    )


####################################################################################################################################
def test_config_render_index():
    """A rule stores an index as a variable length integer, so an index too wide for one byte takes two."""

    # An option list short enough that every index fits one byte
    _, parse = _render()

    assert_in("#define PARSE_RULE_VAL_OPT(value)                                   PARSE_RULE_U32_1(cfgOpt##value)\n", parse)

    # An option list long enough that it does not
    option = OPTION + "".join("  extra-%03u:\n    section: global\n    type: boolean\n" % idx for idx in range(130))
    _, parse = _render(option)

    assert_in("#define PARSE_RULE_VAL_OPT(value)                                   PARSE_RULE_U32_2(cfgOpt##value)\n", parse)


####################################################################################################################################
def test_config_render_label():
    """Each rule is labelled with what it came from, so the generated file can be read."""

    _, parse = _render(label=True)

    for line in parse.split("\n"):
        if line.startswith('        PARSE_RULE_OPTION_NAME("force")'):
            # The label sits at the right margin, which is the width the rest of the source uses
            assert_equal(len(line), 132)
            assert_true(line.endswith("// opt/force"))

            break
    else:
        raise ToolError("unable to find the option to check the label of")

    # A line already too long for the comment is left alone rather than wrapped
    assert_in("                    PARSE_RULE_VAL_STR(QT_this_DS_is_DS_a_DS_very_DS_long", parse)

    # Without labels every line is the rule alone
    _, parse = _render()

    assert_not_in("// opt/force", parse)


####################################################################################################################################
def test_config_render_error():
    """A sequence whose default is not one of the values it numbers cannot be rendered."""

    with assert_raises(ToolError) as error:
        _render(
            """  stanza:
    type: string

  start-fast:
    section: global
    type: boolean
    bool-like: true
    default: false

  target:
    type: string
    command:
      restore: {}

  target-action:
    section: global
    type: string-id
    sequence: true
    default: pause
    allow-list:
      - pause
      - promote

  repo-cipher-type:
    section: global
    type: string-id
    sequence: true
    default: bogus
    allow-list:
      - none
"""
        )

    assert_equal(str(error.exception), "unable to find default 'bogus' in allow list")


####################################################################################################################################
def test_config_render_value_error():
    """A declaration with no value of a type cannot be rendered, since the array the rules index into cannot be empty."""

    with assert_raises(ToolError) as error:
        _render("  stanza:\n    type: string\n")

    assert_equal(str(error.exception), "declaration has no string value for the rules to index")
