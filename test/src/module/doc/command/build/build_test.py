"""Test Documentation Build.

Each renderer has its own test, so what is checked here is that the build reads what it should, writes what it should, and loads the
variables the caller gave before the documents that use them."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from command.build.build import *
from common.error import *
from common.storage import file_read, file_write

CONFIG = """command:
  backup: {}

optionGroup:
  repo: {}

option:
  stanza:
    type: string
"""

HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <description>Configuration description.</description>

        <text><p>Introduction.</p></text>

        <config-section-list/>
    </config>

    <operation title="Command Reference">
        <description>Command description.</description>

        <text><p>Introduction.</p></text>

        <operation-general title="General Options">
            <option-list>
                <option id="stanza" name="Stanza">
                    <summary>Stanza name.</summary>

                    <text><p>Description.</p></text>
                </option>
            </option-list>
        </operation-general>

        <command-list>
            <command id="backup" name="Backup">
                <summary>Back up a cluster.</summary>

                <text><p>Description.</p></text>
            </command>
        </command-list>
    </operation>
</doc>
"""

INDEX = """<doc title="{[project]}" subtitle="Reliable Backup">
    <description>{[project]} backs things up.</description>
</doc>
"""

USER_GUIDE = """<doc title="User Guide">
    <variable key="host">local</variable>

    <p>Running on {[host]} in {[mode]} mode.</p>

    <p if="'{[mode]}' eq 'debug'">Debug only.</p>
</doc>
"""


####################################################################################################################################
def _build(var_map=None, user_guide=USER_GUIDE):
    """Build the documentation and return what was written."""

    with tempfile.TemporaryDirectory() as path:
        file_write(os.path.join(path, "build/config.yaml"), CONFIG)
        file_write(os.path.join(path, "doc/xml/index.xml"), INDEX)
        file_write(os.path.join(path, "doc/xml/reference.xml"), HELP)
        file_write(os.path.join(path, "doc/xml/user-guide.xml"), user_guide)

        cmd_build(path, {"mode": "release"} if var_map is None else var_map)

        return {
            name: file_read(os.path.join(path, sub, name))
            for sub, name in (
                ("doc/output/xml", "command.xml"),
                ("doc/output/xml", "configuration.xml"),
                ("doc/output/xml", "user-guide.xml"),
                ("doc/output/man", "pgbackrest.1.txt"),
            )
        }


####################################################################################################################################
def test_cfg_load():
    """The repository is the one the tool is part of unless the caller names another, and variables are given as key=value."""

    config = cfg_load([], "/repo")

    assert_equal(config.repo_path, "/repo")
    assert_equal(config.var_map, {})
    assert_equal(config.log_level, "info")
    assert_true(config.log_timestamp)

    # A relative path is made absolute so it does not depend on where the tool was run from
    assert_equal(cfg_load(["--repo-path=repo"], "/repo").repo_path, os.path.join(os.getcwd(), "repo"))

    # Variables accumulate, and a value may itself hold an equals sign
    config = cfg_load(["--var=debug=n", "--var=expr=a=b", "--var=empty=", "--log-level=detail", "--no-log-timestamp"], "/repo")

    assert_equal(config.var_map, {"debug": "n", "expr": "a=b", "empty": ""})
    assert_equal(config.log_level, "detail")
    assert_false(config.log_timestamp)


####################################################################################################################################
def test_cfg_load_error():
    """A variable that is not a key and a value is reported, since it would otherwise be silently dropped."""

    for var in ("bogus", "=value"):
        with assert_raises(ToolError) as error:
            cfg_load(["--var=%s" % var], "/repo")

        assert_equal(str(error.exception), "variable '%s' must be given as key=value" % var)


####################################################################################################################################
def test_build():
    """Every document is written from the declarations."""

    output = _build()

    assert_in('subtitle="Command Reference"', output["command.xml"])
    assert_in('subtitle="Configuration Reference"', output["configuration.xml"])
    assert_in('<doc title="User Guide">', output["user-guide.xml"])
    assert_in("pgBackRest - Reliable Backup", output["pgbackrest.1.txt"])


####################################################################################################################################
def test_build_variable():
    """A variable the caller gave is used by the user guide, and a document cannot override it."""

    # The condition is resolved with the value the caller gave, so the debug paragraph is not in a release build
    output = _build()

    assert_not_in("Debug only.", output["user-guide.xml"])

    output = _build({"mode": "debug"})

    assert_in("Debug only.", output["user-guide.xml"])

    # A variable the document declares is available too, and what the caller gave wins over what the document says
    output = _build({"mode": "release", "host": "remote"})

    assert_in('<variable key="host">local</variable>', output["user-guide.xml"])
    assert_in("Running on {[host]} in {[mode]} mode.", output["user-guide.xml"])


####################################################################################################################################
def test_build_error():
    """A user guide that cannot be preprocessed is reported."""

    # A condition needing a variable the caller did not give cannot be evaluated, so it is reported rather than treated as false
    with assert_raises(ToolError) as error:
        _build({})

    assert_in("unreplaced variable in expression", str(error.exception))
