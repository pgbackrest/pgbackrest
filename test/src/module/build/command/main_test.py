"""Test Build Command.

Each generator has its own test, so what is checked here is that the command line reaches the right one and that it writes where it was
told to."""

####################################################################################################################################
import io
import os
import tempfile
from contextlib import redirect_stderr

from harness.test import *

from common.error import *
from common.storage import file_write
from command.main import *

# The smallest declarations each generator will accept, since what each of them renders is checked with the generator itself
ERROR = "assert:\n  code: 25\n  fatal: true\n"

CONFIG = """command:
  backup: {}

optionGroup:
  pg: {}

option:
  buffer-size:
    type: size
    default: 1MiB

  io-timeout:
    type: time
    default: 60

  process-max:
    type: integer
    default: 1

  stanza:
    type: string
"""

HELP = """<doc title="Reference">
    <config title="Configuration Reference">
        <config-section-list/>
    </config>

    <operation title="Command Reference">
        <operation-general title="General Options">
            <option-list>
                <option id="buffer-size" name="Buffer Size">
                    <summary>Buffer size.</summary>

                    <text>
                        <p>Buffer size.</p>
                    </text>
                </option>

                <option id="io-timeout" name="Io Timeout">
                    <summary>Io timeout.</summary>

                    <text>
                        <p>Io timeout.</p>
                    </text>
                </option>

                <option id="process-max" name="Process Max">
                    <summary>Process max.</summary>

                    <text>
                        <p>Process max.</p>
                    </text>
                </option>

                <option id="stanza" name="Stanza">
                    <summary>Stanza.</summary>

                    <text>
                        <p>Stanza.</p>
                    </text>
                </option>
            </option-list>
        </operation-general>

        <command-list>
            <command id="backup" name="Backup">
                <summary>Backup.</summary>

                <text>
                    <p>Backup.</p>
                </text>
            </command>
        </command-list>
    </operation>
</doc>
"""

POSTGRES = "version:\n  - 10\n"
VENDOR = "typedef uint32 TransactionId;\n"
INTERN = "#define PG_INTERFACE_CONTROL_IS(version)\n"


####################################################################################################################################
def _repo_create(path):
    """Write every declaration the generators read."""

    file_write(os.path.join(path, "build/error.yaml"), ERROR)
    file_write(os.path.join(path, "build/config.yaml"), CONFIG)
    file_write(os.path.join(path, "build/postgres.yaml"), POSTGRES)
    file_write(os.path.join(path, "doc/xml/reference.xml"), HELP)
    file_write(os.path.join(path, "src/postgres/interface/version.vendor.h"), VENDOR)
    file_write(os.path.join(path, "src/postgres/interface/version.intern.h"), INTERN)


####################################################################################################################################
def test_cfg_load():
    """The repository is the one the tool is part of unless the caller names another, and the generated code goes there too."""

    config = cfg_load(["error"], "/repo")

    assert_equal(config.command, "error")
    assert_equal(config.repo_path, "/repo")
    assert_equal(config.build_path, "/repo")

    # The documentation generates from its own declarations, so it names the repository to read them from
    config = cfg_load(["config", "--repo-path=/repo/doc"], "/repo")

    assert_equal(config.repo_path, "/repo/doc")
    assert_equal(config.build_path, "/repo/doc")

    # Meson sends the code it generates as it builds to the build directory rather than to the repository
    config = cfg_load(["help", "--repo-path=/repo", "--build-path=/build"], "/repo")

    assert_equal(config.repo_path, "/repo")
    assert_equal(config.build_path, "/build")


####################################################################################################################################
def test_cfg_load_error():
    """A command that has no generator is refused with the list of the ones that do."""

    output = io.StringIO()

    with assert_raises(SystemExit):
        with redirect_stderr(output):
            cfg_load(["bogus"], "/repo")

    assert_in("invalid choice: 'bogus'", output.getvalue())


####################################################################################################################################
def test_cmd_build():
    """The command line reaches the generator it names, which writes where it was told to."""

    generated = {
        "config": ("src/config/config.auto.h", "src/config/parse.auto.c.inc"),
        "error": ("src/common/error/error.auto.h", "src/common/error/error.auto.c.inc"),
        "help": ("src/command/help/help.auto.c.inc",),
        "help-data": ("help.dat",),
        "postgres": ("src/postgres/interface.auto.c.inc",),
        "postgres-version": ("src/postgres/version.auto.h",),
    }

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)

        for command, file_list in generated.items():
            path_build = os.path.join(path, "out", command)

            cmd_build(cfg_load([command, "--repo-path=%s" % path, "--build-path=%s" % path_build], path))

            for file in file_list:
                assert_true(os.path.isfile(os.path.join(path_build, file)), "%s did not generate %s" % (command, file))


####################################################################################################################################
def test_cmd_build_missing():
    """A declaration that is not there is reported with the path it was looked for at."""

    with tempfile.TemporaryDirectory() as path:
        with assert_raises(ToolError) as error:
            cmd_build(cfg_load(["error", "--repo-path=%s" % path], path))

        assert_equal(str(error.exception), "unable to open file '%s/build/error.yaml' for read" % path)
