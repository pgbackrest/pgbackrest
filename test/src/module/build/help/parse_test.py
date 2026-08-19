"""Test Help Parse.

The help is checked against the configuration, so both are written out here. The configuration is the smallest one that has an
option documented in each of the three places help can be: a configuration section, the command line, and a single command."""

####################################################################################################################################
import os
import tempfile
import xml.etree.ElementTree as etree

from harness.test import *

from common.error import *
from common.storage import file_write
from config.parse import bld_cfg_parse
from help.parse import *

# Where the help declaration lives, which the tools tell the parser rather than the parser assuming
PATH_HELP = "doc/xml/reference.xml"

CONFIG = """command:
  backup:
    command-role:
      local: {}
  help: {}
  restore: {}
  version: {}

optionGroup:
  pg: {}

option:
  buffer-size:
    section: global
    type: size
    allow-list:
      - 1MiB
      - 32KiB

  buffer-size-file:
    inherit: buffer-size

  force:
    type: boolean

  stanza:
    type: string

  set:
    type: string
    allow-list:
      - latest
      - oldest
    command:
      backup: {}
      restore: {}

  type:
    type: string
    allow-list:
      - full
      - diff
    command:
      restore:
        allow-list:
          - full
          - diff
          - incr

  process:
    type: integer
    command-role:
      local: {}
    command:
      backup: {}
"""

# Help for everything the configuration declares. Buffer size is documented in a configuration section, force and stanza on the
# command line, and set and type only by the commands that take them. File buffer size allows what buffer size allows, so it
# inherits the list rather than describing it again, and restore inherits the list backup describes for set the same way, naming
# the command since an option documented by more than one has a list per command. Process has no help at all, since no user can
# give it.
HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <text>
            <p>Configuration introduction.</p>
        </text>

        <config-section-list>
            <config-section id="general" name="General">
                <text>
                    <p>Section introduction.</p>
                </text>

                <config-key-list>
                    <config-key id="buffer-size" name="Buffer Size">
                        <summary>Buffer size.</summary>

                        <text>
                            <p>Buffer size description.</p>

                            <allow-list caption="buffer sizes">
                                <allow-item id="1MiB">One <b>MiB</b>.</allow-item>
                                <allow-item id="32KiB">Thirty two KiB.</allow-item>
                            </allow-list>
                        </text>

                        <example>1MiB</example>
                        <example>32KiB</example>
                    </config-key>

                    <config-key id="buffer-size-file" name="File Buffer Size">
                        <summary>File buffer size.</summary>

                        <text>
                            <p>File buffer size description.</p>

                            <allow-list inherit="buffer-size"/>
                        </text>
                    </config-key>
                </config-key-list>
            </config-section>
        </config-section-list>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <text>
            <p>Command introduction.</p>
        </text>

        <operation-general title="General Options">
            <option-list>
                <option id="force" name="Force">
                    <summary>Force.</summary>

                    <text>
                        <p>Force description.</p>
                    </text>
                </option>

                <option id="stanza" name="Stanza" section="stanza">
                    <summary>Stanza.</summary>

                    <text>
                        <p>Stanza description.</p>
                    </text>
                </option>
            </option-list>
        </operation-general>

        <command-list>
            <command id="restore" name="Restore">
                <summary>Restore summary.</summary>

                <text>
                    <p>Restore description.</p>
                </text>

                <option-list>
                    <option id="set" name="Set">
                        <summary>Set summary.</summary>

                        <text>
                            <p>Set description.</p>

                            <allow-list inherit="set" command="backup"/>
                        </text>
                    </option>

                    <option id="type" name="Type">
                        <summary>Type summary.</summary>

                        <text>
                            <p>Type description. Allowed values are <allow-list/>.</p>
                        </text>
                    </option>
                </option-list>
            </command>

            <command id="backup" name="Backup">
                <summary>Backup summary.</summary>

                <text>
                    <p>Backup description.</p>
                </text>

                <option-list>
                    <option id="set" name="Set">
                        <summary>Set summary.</summary>

                        <text>
                            <allow-list caption="sets">
                                <allow-item id="latest">The latest.</allow-item>
                                <allow-item id="oldest">The oldest.</allow-item>
                            </allow-list>
                        </text>
                    </option>
                </option-list>
            </command>

            <command id="help" name="Help">
                <summary>Help summary.</summary>

                <text>
                    <p>Help description.</p>
                </text>
            </command>

            <command id="version" name="Version">
                <summary>Version summary.</summary>

                <text>
                    <p>Version description.</p>
                </text>
            </command>
        </command-list>
    </operation>
</doc>
"""


####################################################################################################################################
def _parse(help=HELP, config=CONFIG, detail=True):
    """Parse a help declaration against a configuration."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), config)
        file_write(os.path.join(path, PATH_HELP), help)

        return bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg_parse(path), detail)


####################################################################################################################################
def _error(help=HELP, config=CONFIG):
    """Parse a help declaration that is expected to fail and return the message."""

    with assert_raises(ToolError) as error:
        _parse(help, config)

    return str(error.exception)


####################################################################################################################################
def _find(item_list, name):
    """Find an entry in a list by name."""

    return next(item for item in item_list if item.name == name)


####################################################################################################################################
def _render(node):
    """Render a node as the xml text of it."""

    return etree.tostring(node, encoding="unicode")


####################################################################################################################################
def test_help_parse():
    """Help is read for every command and option, wherever it is documented."""

    bld_hlp = _parse()

    assert_equal(bld_hlp.cmd_title, "Command Reference")
    assert_equal(bld_hlp.cmd_description, "Command description.")
    assert_equal(bld_hlp.opt_title, "Configuration Reference")
    assert_equal(bld_hlp.opt_description, "Configuration description.")

    # The introductions are left as nodes, since the same nodes are rendered as console text and as documentation
    assert_equal(bld_hlp.cmd_introduction.tag, "text")
    assert_equal(bld_hlp.opt_introduction.tag, "text")

    # Commands are sorted, whatever order they were documented in
    assert_equal([cmd.name for cmd in bld_hlp.cmd_list], ["backup", "help", "restore", "version"])

    cmd = _find(bld_hlp.cmd_list, "restore")

    assert_equal(cmd.title, "Restore")
    assert_equal(cmd.summary.tag, "summary")

    # An option a command documents differently is under the command rather than in the option list
    assert_equal([opt.name for opt in cmd.opt_list], ["set", "type"])
    assert_is_none(_find(bld_hlp.cmd_list, "help").opt_list)

    # Options are sorted and are found wherever they were documented
    assert_equal([opt.name for opt in bld_hlp.opt_list], ["buffer-size", "buffer-size-file", "force", "stanza"])

    # An option documented in a configuration section takes the section it is documented in
    opt = _find(bld_hlp.opt_list, "buffer-size")

    assert_equal(opt.section, "general")
    assert_equal(opt.title, "Buffer Size")
    assert_equal(opt.example_list, ["1MiB", "32KiB"])

    # An option documented on the command line is in no section, unless it says which section it belongs to
    assert_is_none(_find(bld_hlp.opt_list, "force").section)
    assert_is_none(_find(bld_hlp.opt_list, "force").example_list)
    assert_equal(_find(bld_hlp.opt_list, "stanza").section, "stanza")


####################################################################################################################################
def test_help_parse_section():
    """A configuration section is where the options that can be set in the configuration file are documented."""

    bld_hlp = _parse()

    assert_equal([(section.id, section.name) for section in bld_hlp.sct_list], [("general", "General")])
    assert_equal(bld_hlp.sct_list[0].introduction.tag, "text")


####################################################################################################################################
def test_help_parse_detail():
    """The text only the documentation renders is not required when it is not going to be rendered."""

    help = HELP.replace("<description>Command description.</description>", "")
    help = help.replace(
        """        <text>
            <p>Command introduction.</p>
        </text>
""",
        "",
    )
    help = help.replace(
        """                <text>
                    <p>Section introduction.</p>
                </text>
""",
        "",
    )

    bld_hlp = _parse(help, detail=False)

    assert_is_none(bld_hlp.cmd_description)
    assert_is_none(bld_hlp.cmd_introduction)
    assert_is_none(bld_hlp.sct_list[0].introduction)

    # It is required when it is
    assert_in("unable to find child 'text' in node 'config-section'", _error(help))


####################################################################################################################################
def test_help_parse_allow_list():
    """The values an option allows are declared in the configuration, so the help describes them and they are checked against it."""

    bld_hlp = _parse()

    # A list that describes each value is introduced by what it is a list of and rendered as the value then what it means
    buffer_size_list = (
        "<p>The following buffer sizes are supported:</p>"
        "<list><list-item><id>1MiB</id> - One <b>MiB</b>.</list-item>"
        "<list-item><id>32KiB</id> - Thirty two KiB.</list-item></list>"
    )

    assert_in(buffer_size_list, _render(_find(bld_hlp.opt_list, "buffer-size").description))

    # A list that describes no value is rendered from the declaration, in the sentence that introduces the values. An option a
    # command documents is rendered from what the option allows, or from what the command allows when it allows something else.
    cmd_hlp = _find(bld_hlp.cmd_list, "restore")

    assert_in(
        "<p>Type description. Allowed values are <id>full</id>, <id>diff</id>, and <id>incr</id>.</p>",
        _render(_find(cmd_hlp.opt_list, "type").description),
    )

    # An option that allows what another option allows renders the list it inherits rather than one of its own
    assert_in(buffer_size_list, _render(_find(bld_hlp.opt_list, "buffer-size-file").description))

    # An option that more than one command documents renders the list the named command describes, in both places
    set_list = (
        "<p>The following sets are supported:</p>"
        "<list><list-item><id>latest</id> - The latest.</list-item>"
        "<list-item><id>oldest</id> - The oldest.</list-item></list>"
    )

    assert_in(set_list, _render(_find(_find(bld_hlp.cmd_list, "backup").opt_list, "set").description))
    assert_in(set_list, _render(_find(cmd_hlp.opt_list, "set").description))

    # A list inherited from an option that describes none
    assert_equal(
        _error(HELP.replace('<allow-list inherit="buffer-size"/>', '<allow-list inherit="stanza"/>')),
        "option 'buffer-size-file' inherits the allow list of option 'stanza', which does not describe one",
    )

    # A list inherited from a command that describes none
    assert_equal(
        _error(HELP.replace('command="backup"', 'command="help"')),
        "option 'set' for command 'restore' inherits the allow list of option 'set' for command 'help', which does not describe"
        " one",
    )

    # A value that is documented but not allowed
    assert_equal(
        _error(HELP.replace('<allow-item id="32KiB">', '<allow-item id="64KiB">')),
        "option 'buffer-size' allow list is documented as '1MiB, 64KiB' but declared as '1MiB, 32KiB'",
    )

    # A value that is allowed but not documented
    assert_equal(
        _error(HELP.replace('<allow-item id="32KiB">Thirty two KiB.</allow-item>', "")),
        "option 'buffer-size' allow list is documented as '1MiB' but declared as '1MiB, 32KiB'",
    )

    # A list that describes each value but does not say what it is a list of
    assert_equal(
        _error(HELP.replace('<allow-list caption="buffer sizes">', "<allow-list>")),
        "unable to find attribute 'caption' in node 'allow-list'",
    )

    # An allow list documented for an option that does not have one
    assert_equal(
        _error(HELP.replace("<p>Force description.</p>", "<p>Force description.</p><allow-list/>")),
        "option 'force' does not have an allow list",
    )

    # An allow list that is not documented at all, since a value a user cannot read about is a value they cannot use
    assert_equal(
        _error(HELP.replace("Allowed values are <allow-list/>.", "")),
        "option 'type' for command 'restore' must document its allow list",
    )


####################################################################################################################################
def test_help_parse_error():
    """Anything the user can reach must have help, since help that is missing is only found by looking for it."""

    # A command with no help at all
    assert_equal(
        _error(HELP.replace('<command id="backup" name="Backup">', '<command id="bogus" name="Bogus">')),
        "command 'backup' must have help",
    )

    # An option with no help of its own must be documented by every command that takes it
    assert_equal(
        _error(HELP.replace('<option id="set" name="Set">', '<option id="bogus" name="Bogus">')),
        "option 'set' must have help for command 'backup'",
    )

    # Help for a command that does not exist, which is a leftover rather than something missing
    assert_in(
        "unable to find attribute 'id' in node 'option'",
        _error(HELP.replace('<option id="force" name="Force">', '<option name="Force">')),
    )
