"""Test Command and Configuration Reference.

The declarations are written out here rather than read from the repository, since what needs testing is how an option is rendered rather
than what the project happens to declare. The rendered documents are checked as fragments of the xml that comes out.

An option appears in both references and is rendered differently in each -- an example is a command line in the command reference and a
configuration file line in the configuration reference -- so most options here are checked in both."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from command.build.reference import *
from common.storage import file_write
from config.parse import bld_cfg_parse
from help.parse import bld_hlp_parse

# Where the help declaration lives, which the tools tell the parser rather than the parser assuming
PATH_HELP = "doc/xml/reference.xml"

CONFIG = """command:
  backup: {}

  expire:
    internal: true

  help: {}

  restore: {}

  version: {}

optionGroup:
  repo: {}

option:
  archive-copy:
    section: global
    type: boolean
    default: false

  cmd:
    section: global
    type: string
    default-type: dynamic
    default: bin

  compress-level:
    section: global
    type: integer
    allow-range:
      - none: [0, 0]
      - gz: [0, 9]
    depend:
      option: compress-type

  compress-type:
    section: global
    type: string-id
    default: gz

  delta:
    type: boolean
    command:
      restore: {}

  force:
    type: boolean
    internal: true

  online:
    section: global
    type: boolean
    default: true

  process-max:
    section: global
    type: integer
    default: 1
    allow-range: [1, 999]

  repo-path:
    section: global
    group: repo
    type: path
    default: /var/lib/pgbackrest
    deprecate:
      repo-path: {}
      db-path: {}

  repo-cipher-pass:
    section: global
    group: repo
    type: string
    secure: true
    deprecate:
      repo-cipher-pass: {}

  repo-storage-port:
    section: global
    group: repo
    type: integer
    default:
      - s3: 443
      - azure: 443
    depend:
      option: compress-type

  set:
    type: string
    beta: true
    command:
      restore: {}

  spool-path:
    section: global
    type: path
    command:
      backup:
        internal: true
      restore: {}

  stanza:
    type: string

  target:
    type: string
    command-role:
      local: {}
    command:
      restore: {}
"""


####################################################################################################################################
def _option(id, name, section=None, summary="Summary.", example=None, tag="option"):
    """Help for one option.

    A configuration section documents an option as a config-key, so the tag varies with where the help is written."""

    return '<%s id="%s" name="%s"%s><summary>%s</summary><text><p>Description of %s.</p></text>%s</%s>' % (
        tag,
        id,
        name,
        "" if section is None else ' section="%s"' % section,
        summary,
        id,
        "" if example is None else "".join("<example>%s</example>" % value for value in example),
        tag,
    )


####################################################################################################################################
HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <text><p>Configuration introduction.</p></text>

        <config-section-list>
            <config-section id="general" name="General">
                <text><p>General introduction.</p></text>

                <config-key-list>
                    %s
                </config-key-list>
            </config-section>

            <config-section id="repository" name="Repository">
                <text><p>Repository introduction.</p></text>

                <config-key-list>
                    %s
                </config-key-list>
            </config-section>
        </config-section-list>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <text><p>Command introduction.</p></text>

        <operation-general title="General Options">
            <option-list>
                %s
            </option-list>
        </operation-general>

        <command-list>
            %s
        </command-list>
    </operation>
</doc>
""" % (
    "".join(
        (
            _option("archive-copy", "Archive Copy", tag="config-key"),
            _option("cmd", "Cmd", tag="config-key"),
            _option("compress-level", "Compress Level", tag="config-key"),
            _option("compress-type", "Compress Type", example=["lz4"], tag="config-key"),
            _option("online", "Online", example=["n"], tag="config-key"),
            _option("process-max", "Process Max", tag="config-key"),
            _option("spool-path", "Spool Path", tag="config-key"),
        )
    ),
    "".join(
        (
            _option("repo-path", "Repo Path", example=["/backup", "/other"], tag="config-key"),
            _option("repo-cipher-pass", "Repo Cipher Pass", tag="config-key"),
            _option("repo-storage-port", "Repo Storage Port", tag="config-key"),
        )
    ),
    "".join(
        (
            _option("delta", "Delta", example=["y"]),
            _option("force", "Force"),
            _option("stanza", "Stanza"),
            _option("target", "Target"),
        )
    ),
    "".join(
        '<command id="%s" name="%s"><summary>%s summary.</summary><text><p>%s description.</p></text>%s</command>'
        % (id, name, name, name, option)
        for id, name, option in (
            ("backup", "Backup", ""),
            ("expire", "Expire", ""),
            ("help", "Help", ""),
            ("restore", "Restore", "<option-list>%s</option-list>" % _option("set", "Set", example=["latest"])),
            ("version", "Version", ""),
        )
    ),
)


####################################################################################################################################
def _render():
    """Render both references and return the xml of each."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, PATH_HELP), HELP)

        bld_cfg = bld_cfg_parse(path)
        bld_hlp = bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg, True)

        return reference_command_render(bld_cfg, bld_hlp).render(), reference_configuration_render(bld_cfg, bld_hlp).render()


####################################################################################################################################
def test_reference_document():
    """A reference says what it is and introduces itself before anything else."""

    command, configuration = _render()

    for document, subtitle, description in (
        (command, "Command Reference", "Command description."),
        (configuration, "Configuration Reference", "Configuration description."),
    ):
        assert_in('<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE doc SYSTEM "doc.dtd">\n', document)
        assert_in('<doc title="{[project]}" subtitle="%s" toc="y">' % subtitle, document)
        assert_in("<description>%s</description>" % description, document)
        assert_in('<section id="introduction"><title>Introduction</title><p>', document)


####################################################################################################################################
def test_reference_configuration():
    """The configuration reference documents every option that can be set in the configuration file, under its section."""

    _, configuration = _render()

    # A section holds the options documented under it, and the introduction it was written with
    assert_in('<section id="section-general"><title>General Options</title><p>General introduction.</p>', configuration)
    assert_in('<section id="section-repository"><title>Repository Options</title>', configuration)

    # An option is titled with its name and described with the help it was written with
    assert_in(
        '<section id="option-process-max"><title>Process Max Option (<id>--process-max</id>)</title>'
        "<p>Summary.</p><p>Description of process-max.</p>",
        configuration,
    )

    # An option that cannot be set in the configuration file at all is not here, nor is one that is internal. An option holding a
    # secret is, since a reader still has to set it -- it is the command reference that leaves it out.
    assert_not_in("option-delta", configuration)
    assert_not_in("option-force", configuration)
    assert_in("option-repo-cipher-pass", configuration)
    assert_not_in("option-repo-cipher-pass", _render()[0])

    # An example is a configuration file line, one per line, and an option in a group is shown at the first index
    assert_in("example: repo1-path=/backup\nexample: repo1-path=/other</code-block>", configuration)


####################################################################################################################################
def test_reference_command():
    """The command reference documents every command and, under it, the options that command takes."""

    command, _ = _render()

    assert_in('<section id="command-backup"><title>Backup Command (<id>backup</id>)</title><p>Backup description.</p>', command)

    # A command a reader has no use for is not here
    assert_not_in("command-expire", command)

    # Options are grouped by what they are about, and a section that means nothing outside the command is grouped under one heading
    assert_in('<section id="category-general" toc="n"><title>General Options</title>', command)
    assert_in('<section id="category-repository" toc="n"><title>Repository Options</title>', command)
    assert_in('<section id="category-command" toc="n"><title>Command Options</title>', command)

    # Help a command documents itself wins over the help the option has of its own, and is grouped as a command option
    assert_in("<p>Description of set.</p>", command)

    # An option a command does not take is not under it, nor is one the command marks internal, nor one no user can give
    assert_not_in("option-delta</", command.split('id="command-backup"')[1].split('id="command-help"')[0])

    # An example is a command line, with the options given one after another
    assert_in("example: --compress-type=lz4</code-block>", command)

    # A boolean is given by naming it, so turning it off is naming the negation of it
    assert_in("example: --no-online</code-block>", command)
    assert_in("example: --delta", command)


####################################################################################################################################
def test_reference_option_detail():
    """What a reader needs to use an option is rendered below the description of it."""

    command, configuration = _render()

    # A default, written the way it would be given rather than the way it is declared
    assert_in("<code-block>default: 1", configuration)
    assert_in("<code-block>default: y\n", configuration)
    assert_in("<code-block>default: n</code-block>", configuration)
    assert_in("<code-block>default: /var/lib/pgbackrest", configuration)

    # A default that is worked out when the command runs
    assert_in("<code-block>default: [path of executed pgbackrest binary]</code-block>", configuration)

    # A default per value of the option it depends on
    assert_in("<code-block>default (depending on compress-type):\n    s3 - 443\n    azure - 443</code-block>", configuration)

    # A range, and a range per value of the option it depends on
    assert_in("allowed: [1, 999]", configuration)
    assert_in("allow range (depending on compress-type):\n    none - [0, 0]\n    gz - [0, 9]", configuration)

    # An option that is not ready to be relied on says so
    assert_in("<p>FOR BETA TESTING ONLY. DO NOT USE IN PRODUCTION.</p>", command)

    # Names the option can still be given by, less the one that is the option name itself
    assert_in("<p>Deprecated Names: db-path</p>", configuration)

    # An option whose only deprecation is its own name has no deprecated name to report
    assert_not_in("Deprecated Name:", configuration)
