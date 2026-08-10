"""Test Manual Page Reference.

The wrapping is what most of this is about, so the expectations are written as the exact lines that come out. The width is fixed at
80 and cannot be varied, so the summaries here are written long enough to wrap at it."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from command.build.man import *
from common.error import *
from common.storage import file_write
from common.xml import xml_parse
from config.parse import bld_cfg_parse
from help.parse import bld_hlp_parse

# Where the help declaration lives, which the tools tell the parser rather than the parser assuming
PATH_HELP = "doc/xml/reference.xml"

INDEX = """<doc title="{[project]}" subtitle="Reliable {[postgres]} Backup">
    <description>{[project]} is a backup solution for {[postgres]}.

Reliable and simple.</description>
</doc>
"""

CONFIG = """command:
  backup: {}

  expire:
    internal: true

  help: {}

optionGroup:
  repo: {}

option:
  archive-async:
    section: global
    type: boolean

  buffer-size:
    section: global
    type: size

  force:
    type: boolean
    internal: true

  repo-path:
    section: global
    type: path
    command:
      backup:
        internal: true

  set:
    type: string
    command:
      backup: {}

  spool-path:
    section: global
    type: path
    command:
      backup:
        internal: true
      help: {}

  target:
    type: string
    command:
      help: {}

  stanza:
    type: string
"""

# Summaries long enough to wrap, including one holding a word that is longer than a line on its own
SUMMARY_LONG = (
    "Size of the buffer used for file operations, which should be tuned to the size of the files being backed up so that the "
    "operation is as efficient as it can be."
)

SUMMARY_WORD = "Path such as /var/spool/pgbackrest/archive/replace/this/with/a/really/quite/long/path/that/cannot/be/broken"

# A summary beginning with a word that is longer than a line, which has no space before it to break at
SUMMARY_FIRST = "https://pgbackrest.org/user-guide.html#quickstart/configure-archiving-and-backups has more."


####################################################################################################################################
def _help(id, name, summary, tag="option", section=None):
    """Help for one option or command."""

    return '<%s id="%s" name="%s"%s><summary>%s</summary><text><p>Description.</p></text></%s>' % (
        tag,
        id,
        name,
        "" if section is None else ' section="%s"' % section,
        summary,
        tag,
    )


HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <text><p>Introduction.</p></text>

        <config-section-list>
            <config-section id="general" name="General">
                <text><p>Introduction.</p></text>

                <config-key-list>%s</config-key-list>
            </config-section>
        </config-section-list>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <text><p>Introduction.</p></text>

        <operation-general title="General Options">
            <option-list>%s</option-list>
        </operation-general>

        <command-list>%s</command-list>
    </operation>
</doc>
""" % (
    _help("archive-async", "Archive Async", SUMMARY_FIRST, tag="config-key")
    + _help("buffer-size", "Buffer Size", SUMMARY_LONG, tag="config-key")
    + _help("spool-path", "Spool Path", SUMMARY_WORD, tag="config-key"),
    _help("force", "Force", "Force it.") + _help("stanza", "Stanza", "Stanza name."),
    "".join(
        '<command id="%s" name="%s"><summary>%s</summary><text><p>Description.</p></text>%s</command>' % (id, name, summary, option)
        for id, name, summary, option in (
            (
                "backup",
                "Backup",
                "Back up a database cluster.",
                "<option-list>%s</option-list>"
                % (_help("repo-path", "Repo Path", "Repository path.") + _help("set", "Set", "Set to use.")),
            ),
            ("expire", "Expire", "Expire backups.", ""),
            ("help", "Help", "Get help.", "<option-list>%s</option-list>" % _help("target", "Target", "Target to use.")),
        )
    ),
)


####################################################################################################################################
def _render(help=HELP):
    """Render the manual page."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, PATH_HELP), help)

        bld_cfg = bld_cfg_parse(path)

        return reference_man_render(
            xml_parse(INDEX, "index.xml"), bld_cfg, bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg, True)
        )


####################################################################################################################################
def test_man_header():
    """The manual page says what the project is, taking it from the documentation index."""

    man = _render()

    # The variables the index uses are replaced, and the description is wrapped and indented from the first line
    assert_in(
        """NAME
  pgBackRest - Reliable PostgreSQL Backup

SYNOPSIS
  pgbackrest [options] [command]

DESCRIPTION
  pgBackRest is a backup solution for PostgreSQL.

  Reliable and simple.
""",
        man,
    )


####################################################################################################################################
def test_man_header_error():
    """A variable the manual page does not know is reported rather than rendered as its own name."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, PATH_HELP), HELP)

        bld_cfg = bld_cfg_parse(path)
        bld_hlp = bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg, True)
        index = xml_parse(INDEX.replace("{[postgres]}", "{[bogus]}"), "index.xml")

        with assert_raises(ToolError) as error:
            reference_man_render(index, bld_cfg, bld_hlp)

        assert_in("unreplaced variable(s) in:", str(error.exception))


####################################################################################################################################
def test_man_option_error():
    """Help for an option that is not declared, or that the command does not take, is reported rather than left to fail obscurely.

    Parsing the help only checks that everything declared has help, so a leftover in the other direction reaches here."""

    # An option documented by a command that does not take it, which only the help command does
    help = HELP.replace(
        _help("set", "Set", "Set to use."), _help("target", "Target", "Target.") + _help("set", "Set", "Set to use.")
    )

    with assert_raises(ToolError) as error:
        _render(help)

    assert_equal(str(error.exception), "help for command 'backup' documents option 'target', which the command does not take")

    # An option documented but no longer declared, under a command and in the general option list
    help = HELP.replace(_help("set", "Set", "Set to use."), _help("bogus", "Bogus", "Bogus.") + _help("set", "Set", "Set to use."))

    with assert_raises(ToolError) as error:
        _render(help)

    assert_equal(str(error.exception), "help for command 'backup' documents option 'bogus', which is not declared")

    help = HELP.replace(
        _help("stanza", "Stanza", "Stanza name."), _help("stanza", "Stanza", "Stanza name.") + _help("bogus", "Bogus", "Bogus.")
    )

    with assert_raises(ToolError) as error:
        _render(help)

    assert_equal(str(error.exception), "help in the option list documents option 'bogus', which is not declared")


####################################################################################################################################
def test_man_command():
    """Every command a reader can run is listed with its summary, aligned in a column."""

    man = _render()

    assert_in(
        """COMMANDS
  backup  Back up a database cluster.
  help    Get help.
""",
        man,
    )

    # A command a reader has no use for is not listed
    assert_not_in("expire", man)


####################################################################################################################################
def test_man_option():
    """Every option a reader can set is listed under its section, with the sections in order."""

    man = _render()

    # An option a command documents itself is listed under that command, and an option with no section under General
    assert_in("  Backup Options:\n    --set", man)
    assert_in("  General Options:\n", man)
    assert_in("    --stanza", man)

    # An option a reader has no use for is not listed, nor is one the command documents but marks internal for itself
    assert_not_in("--force", man)
    assert_not_in("--repo-path", man)


####################################################################################################################################
def test_man_wrap():
    """A summary too long for the line is wrapped and the wrapped lines are indented past the option name."""

    man = _render()

    # Every line fits the console width, unless it holds a single word that does not
    for line in man.split("\n"):
        if "/var/spool" not in line and "pgbackrest.org/user-guide" not in line:
            assert_true(len(line) <= 80, "line is %d wide: %r" % (len(line), line))

    # The summary continues under itself rather than under the option name
    assert_in(
        """    --buffer-size    Size of the buffer used for file operations, which should
                     be tuned to the size of the files being backed up so that
                     the operation is as efficient as it can be.
""",
        man,
    )

    # A word longer than the line is left on a line of its own rather than broken, whether it ends the summary or begins it
    assert_in("/var/spool/pgbackrest/archive/replace/this/with/a/really/quite/long/path/that/cannot/be/broken", man)
    assert_in("https://pgbackrest.org/user-guide.html#quickstart/configure-archiving-and-backups\n", man)


####################################################################################################################################
def test_man_footer():
    """The manual page ends with where things are, how to use it, and where to read more."""

    man = _render()

    assert_in("FILES\n  /etc/pgbackrest/pgbackrest.conf\n", man)
    assert_in("EXAMPLES\n  * Create a backup of the PostgreSQL `main` cluster:\n", man)
    assert_in("SEE ALSO\n  /usr/share/doc/pgbackrest-doc/html/index.html\n  https://pgbackrest.org\n", man)
