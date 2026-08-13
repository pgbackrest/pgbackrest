"""Test Help Render.

The rendered help is checked as the console text it becomes, and the packed help by decompressing it again, since what the C reads
back is the only thing that matters about the bytes."""

####################################################################################################################################
import bz2
import os
import tempfile
import xml.etree.ElementTree as etree

from harness.test import *

from common.error import *
from common.storage import file_read, file_write
from config.parse import bld_cfg_parse
from help.parse import bld_hlp_parse
from help.render import *

# Where the help declaration lives, which the tools tell the parser rather than the parser assuming
PATH_HELP = "doc/xml/reference.xml"

CONFIG = """command:
  backup: {}
  help: {}
  restore: {}
  version: {}

optionGroup:
  pg: {}

option:
  force:
    type: boolean
    internal: true
    command:
      backup:
        internal: false
      restore:
        internal: false

  pg-path:
    section: global
    group: pg
    type: path
    deprecate:
      pg-path: {}
      db-path: {}

  set:
    type: string
    command:
      restore: {}

  stanza:
    type: string
"""

HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <config-section-list>
            <config-section id="general" name="General">
                <config-key-list>
                    <config-key id="pg-path" name="Pg Path">
                        <summary>Pg path.</summary>

                        <text>
                            <p>Pg path description.</p>
                        </text>
                    </config-key>
                </config-key-list>
            </config-section>
        </config-section-list>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <operation-general title="General Options">
            <option-list>
                <option id="force" name="Force">
                    <summary>Force.</summary>

                    <text>
                        <p>Force description.</p>
                    </text>
                </option>

                <option id="stanza" name="Stanza">
                    <summary>Stanza.</summary>

                    <text>
                        <p>Stanza description.</p>
                    </text>
                </option>
            </option-list>
        </operation-general>

        <command-list>
            <command id="backup" name="Backup">
                <summary>Backup summary.</summary>

                <text>
                    <p>Backup description.</p>
                </text>
            </command>

            <command id="help" name="Help">
                <summary>Help summary.</summary>

                <text>
                    <p>Help description.</p>
                </text>
            </command>

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
                        </text>
                    </option>
                </option-list>
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
def _xml(content):
    """Build a node from the markup a summary or a description would hold."""

    return etree.fromstring("<text>%s</text>" % content)


####################################################################################################################################
def test_help_render_xml():
    """Markup is rendered as the plain text the console shows."""

    # A paragraph is separated from what follows it, and the whole is trimmed
    assert_equal(bld_hlp_render_xml(_xml("<p>One.</p><p>Two.</p>")), "One.\n\nTwo.")

    # Tags that stand for a name render as the name
    assert_equal(bld_hlp_render_xml(_xml("<p><backrest/> backs up <postgres/>.</p>")), "pgBackRest backs up PostgreSQL.")

    # A note says it is one, since the console has no other way to set it apart
    assert_equal(bld_hlp_render_xml(_xml("<admonition type='note'>Careful.</admonition>Text.")), "NOTE: Careful.\n\nText.")

    # A list is separated from what follows it and each item is marked
    assert_equal(
        bld_hlp_render_xml(_xml("<list><list-item>One</list-item><list-item>Two</list-item></list>Text.")), "* One\n* Two\n\nText."
    )

    # A quote is rendered as the quotes around it
    assert_equal(bld_hlp_render_xml(_xml("<p>Say <quote>hello</quote>.</p>")), 'Say "hello".')

    # A tag that only marks up what it contains renders as its content
    assert_equal(
        bld_hlp_render_xml(_xml("<p>Use <br-option>--force</br-option> in <file>/etc</file>.</p>")), "Use --force in /etc."
    )

    # A dash is written as a variable in the xml so the documentation renderers do not treat it as markup
    assert_equal(bld_hlp_render_xml(_xml("<p>a{[dash]}b</p>")), "a-b")


####################################################################################################################################
def test_help_render_xml_error():
    """Markup that would not render is reported rather than dropped."""

    with assert_raises(ToolError) as error:
        bld_hlp_render_xml(_xml("<p><bogus/></p>"))

    assert_equal(str(error.exception), "unknown tag 'bogus'")

    # Text laid out over more than one line is how the xml is written to be read, so it must be only whitespace
    assert_equal(bld_hlp_render_xml(etree.fromstring("<text>\n    <p>One.</p>\n</text>")), "One.")

    with assert_raises(ToolError) as error:
        bld_hlp_render_xml(etree.fromstring("<text>bogus\n    text<p>One.</p></text>"))

    assert_equal(str(error.exception), "text 'bogus\n    text' is invalid")


####################################################################################################################################
# What each short escape stands for, which is read here the way the C compiler would read it. Python cannot be used to unescape the
# literal since it has no \? escape and would reject it.
ESCAPE = {"a": 0x07, "b": 0x08, "t": 0x09, "n": 0x0A, "v": 0x0B, "f": 0x0C, "r": 0x0D, '"': 0x22, "\\": 0x5C, "?": 0x3F}


####################################################################################################################################
def _unescape(text):
    """Read a C string literal back as the bytes it holds."""

    result = bytearray()
    idx = 0

    while idx < len(text):
        if text[idx] != "\\":
            result.append(ord(text[idx]))
            idx += 1
        elif text[idx + 1] in ESCAPE:
            result.append(ESCAPE[text[idx + 1]])
            idx += 2
        else:
            # Anything else is a three digit octal escape
            result.append(int(text[idx + 1 : idx + 4], 8))
            idx += 4

    return bytes(result)


####################################################################################################################################
def _render():
    """Render the help and return what was written and the pack it holds, decompressed."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, PATH_HELP), HELP)

        bld_cfg = bld_cfg_parse(path)
        bld_hlp_render(path, bld_cfg, bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg, False))

        content = file_read(os.path.join(path, "src/command/help/help.auto.c.inc"))

        # Recover the string literal, which is broken into lines with a continuation the preprocessor would remove
        data = content[content.index('=\n"') + 3 : -len('";\n')].replace("\\\n", "")

        return content, _unescape(data)


####################################################################################################################################
def test_help_render():
    """The help is a C string literal holding the compressed pack the C reads back."""

    content, data = _render()

    assert_in("VR_NON_STRING static const char helpData[", content)

    # Every line is as wide as the rest of the source, counting the continuation
    for line in content.split("\n")[6:-2]:
        assert_equal(len(line), 132)

    # The pack decompresses, which is the only thing that matters about the bytes
    pack = bz2.decompress(data)

    # The help text is in there, so the summaries and descriptions that were rendered are what got packed
    assert_in(b"Backup summary.", pack)
    assert_in(b"Set summary.", pack)
    assert_in(b"Pg path description.", pack)

    # A deprecated name is packed, but not one that is the option name itself since that is not a name to report
    assert_in(b"db-path", pack)
    assert_not_in(b"\x08pg-path", pack)


####################################################################################################################################
def test_help_render_size():
    """The help is only written when it changes, so an unchanged one does not trigger a rebuild."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, PATH_HELP), HELP)

        bld_cfg = bld_cfg_parse(path)
        bld_hlp = bld_hlp_parse(os.path.join(path, PATH_HELP), bld_cfg, False)

        bld_hlp_render(path, bld_cfg, bld_hlp)

        path_help = os.path.join(path, "src/command/help/help.auto.c.inc")
        time_before = os.stat(path_help).st_mtime_ns

        bld_hlp_render(path, bld_cfg, bld_hlp)

        assert_equal(os.stat(path_help).st_mtime_ns, time_before)
