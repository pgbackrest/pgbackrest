"""Test Test Build Handler.

The build is checked by generating a few representative tests against a repository written here and comparing the whole of what
comes out, since the generated build is the thing that has to be right and any change to it should be deliberate.

The paths in the generated files are absolute and the user and group are whoever the tests run as, so both are replaced with tokens
before comparing, the same way the C tests do it."""

####################################################################################################################################
import os
import tempfile
from unittest.mock import patch

from harness.test import *

from command.test.build import *
from command.test.define import test_def_find, test_def_parse
from common.error import *
from common.log import *
from common.storage import path_list_recurse

# The repository the build reads. Only the modules a harness shims are read as source, so those are the only ones written out.
DEFINE = """
unit:
  - name: common

    test:
      - name: error
        total: 2

        coverage:
          - common/error

      - name: exec
        total: 1
        feature: exec

        harness:
          - name: config
            integration: false

            shim:
              - common/execOther
              - name: common/exec
                function:
                  - execOne
                  - execTwo
                  - execThree:
                      inc: execInc
                  - execFour:
                      inc: execInc

        coverage:
          - common/exec
          - common/execOther

        depend:
          - common/log

      - name: after
        total: 1
        harness: extra

        coverage:
          - common/after

integration:
  - name: real

    test:
      - name: all
        total: 1

performance:
  - name: performance

    test:
      - name: type
        total: 1
        define: -DNDEBUG

        coverage:
          - performance/type
"""

MESON_OPTIONS = "option('fake', type: 'boolean', value: false)\n"

MESON = (
    "project('pgbackrest', 'c')\n"
    "configuration = configuration_data()\n"
    "    configuration.set('HAVE_LIBBACKTRACE', true)\n"
    "subdir('src')\n"
    "meson.add_dist_script('test/dist.sh')\n"
)

# A module a harness shims, with an extern function, a static function, and a function defined in an included module
SOURCE_EXEC = (
    "/***/\n"
    '#include "common/execInc.c.inc"\n'
    "\n"
    "FN_EXTERN void\n"
    "execOne(const char *command)\n"
    "{\n"
    "    doExec();\n"
    "}\n"
    "\n"
    "static void\n"
    "execTwo(void)\n"
    "{\n"
    "    doTwo();\n"
    "}\n"
)

SOURCE_EXEC_INC = (
    "/***/\n"
    "FN_EXTERN void\n"
    "execThree(\n"
    "    const char *command,\n"
    "    int flag)\n"
    "{\n"
    "    doThree();\n"
    "}\n"
    "\n"
    "FN_EXTERN void\n"
    "execFour(void)\n"
    "{\n"
    "    doFour();\n"
    "}\n"
)

HARNESS_CONFIG = "/***/\n{[SHIM_MODULE]}\n\nvoid\nhrnConfig(void)\n{\n}\n"
HARNESS_EXTRA = "/***/\nvoid\nhrnExtra(void)\n{\n}\n"
HARNESS_HELPER = "/***/\nvoid\nhrnHelper(void)\n{\n}\n"

# The test.c the build fills in, holding every substitution it makes
TEST_C = (
    "/***/\n"
    "{[C_TEST_DEBUG_TEST_TRACE]}\n"
    "{[C_INCLUDE]}\n"
    '#define TEST_PATH "{[C_TEST_PATH]}"\n'
    '#define HRN_PATH "{[C_HRN_PATH]}"\n'
    '#define HRN_PATH_REPO "{[C_HRN_PATH_REPO]}"\n'
    '#define TEST_PROJECT_EXE "{[C_TEST_PROJECT_EXE]}"\n'
    '#define TEST_PGB_PATH "{[C_TEST_PGB_PATH]}"\n'
    "#define TEST_LOG_EXPECT {[C_TEST_LOG_EXPECT]}\n"
    "#define LOG_LEVEL_TEST {[C_LOG_LEVEL_TEST]}\n"
    "#define TEST_TIMING {[C_TEST_TIMING]}\n"
    '#define TEST_ARCHITECTURE "{[C_TEST_ARCHITECTURE]}"\n'
    "{[C_TEST_TZ]}\n"
    "#define TEST_SCALE {[C_TEST_SCALE]}\n"
    "#define TEST_CONTAINER {[C_TEST_CONTAINER]}\n"
    '#define TEST_GROUP "{[C_TEST_GROUP]}"\n'
    "#define TEST_GROUP_ID {[C_TEST_GROUP_ID]}\n"
    '#define TEST_USER "{[C_TEST_USER]}"\n'
    "#define TEST_USER_LEN {[C_TEST_USER_LEN]}\n"
    "#define TEST_USER_ID {[C_TEST_USER_ID]}\n"
    '#define TEST_VM "{[C_TEST_VM]}"\n'
    '#define TEST_PG_VERSION "{[C_TEST_PG_VERSION]}"\n'
    "#define TEST_IDX {[C_TEST_IDX]}\n"
    "{[C_TEST_INCLUDE]}\n"
    '#define TEST_PATH_BUILD "{[C_TEST_PATH_BUILD]}"\n'
    "\n"
    "void\n"
    "testRun(void)\n"
    "{\n"
    "    {[C_TEST_LIST]}\n"
    "}\n"
)

# The user and group the generated test.c is built with, so it does not depend on who ran the tests
USER = "pgbackrest"
GROUP = "pgbackrest"
USER_ID = 1000
GROUP_ID = 1001


####################################################################################################################################
class Config:
    """What the build reads from the command line."""

    def __init__(self, repo_path, test_path, **option):
        self.repo_path = repo_path
        self.test_path = test_path
        self.vm = "none"
        self.vm_id = 0
        self.pg_version = "invalid"
        self.test = None
        self.scale = 1
        self.tz = None
        self.log_level_test = OFF
        self.log_timestamp = True
        self.coverage = True
        self.back_trace = True
        self.optimize = False
        self.profile = False

        for name, value in option.items():
            setattr(self, name, value)


####################################################################################################################################
def _repo_create(path):
    """Write the repository the build reads."""

    result = os.path.join(path, "repo")

    for name, content in (
        ("meson_options.txt", MESON_OPTIONS),
        ("meson.build", MESON),
        ("test/define.yaml", DEFINE),
        ("test/src/test.c", TEST_C),
        ("src/common/exec.c", SOURCE_EXEC),
        ("src/common/execInc.c.inc", SOURCE_EXEC_INC),
        ("test/src/harness/config.c", HARNESS_CONFIG),
        ("test/src/harness/config/helper.c", HARNESS_HELPER),
        ("test/src/harness/extra.c", HARNESS_EXTRA),
    ):
        os.makedirs(os.path.dirname(os.path.join(result, name)), exist_ok=True)

        with open(os.path.join(result, name), "w") as file:
            file.write(content)

    return result


####################################################################################################################################
def _build(path, module_name, architecture="x86_64", **option):
    """Generate the unit build for a module, returning the unit path and the files that were generated."""

    path_repo = os.path.join(path, "repo")
    config = Config(path_repo, path, **option)
    module = test_def_find(test_def_parse(path_repo), module_name)

    # The user and group are whoever the tests run as, so they are fixed here to keep the generated test.c the same everywhere
    with patch("command.test.build.user_name", lambda: USER), patch("command.test.build.group_name", lambda: GROUP), patch(
        "command.test.build.os.getuid", lambda: USER_ID
    ), patch("command.test.build.os.getgid", lambda: GROUP_ID):
        test_build = TestBuild(config, module, architecture)
        test_build.build()

    return test_build.path_unit, path_list_recurse(test_build.path_unit)


####################################################################################################################################
def _read(path, path_unit, name):
    """Read a generated file with the paths that change from run to run replaced by tokens."""

    with open(os.path.join(path_unit, name)) as file:
        content = file.read()

    return content.replace(os.path.join(path, "repo"), "[REPO_PATH]").replace(path, "[TEST_PATH]")


####################################################################################################################################
# The part of meson.build that the build appends, which is the same for every module apart from the arguments and the sources
MESON_CONFIGURE = (
    "\n"
    + MESON_COMMENT_BLOCK
    + "\n# Write configuration\n"
    + MESON_COMMENT_BLOCK
    + "\n"
    + "configure_file(output: 'build.auto.h', configuration: configuration)\n"
    + "\n"
    + "add_global_arguments('-DFN_EXTERN=extern', language : 'c')\n"
    + "add_global_arguments('-DVR_EXTERN_DECLARE=extern', language : 'c')\n"
    + "add_global_arguments('-DVR_EXTERN_DEFINE=', language : 'c')\n"
    + "add_global_arguments('-DERROR_MESSAGE_BUFFER_SIZE=131072', language : 'c')\n"
)

MESON_UNIT = "\n" + MESON_COMMENT_BLOCK + "\n# Unit test\n" + MESON_COMMENT_BLOCK + "\n" + "src_unit = files(\n"

MESON_EXECUTABLE = (
    "    '../../repo/test/src/harness/test.c',\n"
    "    'test.c',\n"
    ")\n"
    "\n"
    "executable(\n"
    "    'test-unit',\n"
    "    sources: src_unit,\n"
)

MESON_INCLUDE = (
    "    include_directories:\n"
    "        include_directories(\n"
    "            '.',\n"
    "            '../../repo/src',\n"
    "            '../../repo/test/src',\n"
    "        ),\n"
    "    dependencies: [\n"
)

MESON_LIB = (
    "        lib_bz2,\n"
    "        lib_openssl,\n"
    "        lib_lz4,\n"
    "        lib_pq,\n"
    "        lib_ssh2,\n"
    "        lib_xml,\n"
    "        lib_z,\n"
    "        lib_zstd,\n"
    "    ],\n"
    ")\n"
)

# meson.build as the build leaves the part it read from the repository
MESON_READ = (
    "project('pgbackrest', 'c')\n"
    "configuration = configuration_data()\n"
    "    configuration.set('HAVE_LIBBACKTRACE', true)\n"
    "# subdir('src')\n"
    "# meson.add_dist_script('test/dist.sh')\n"
)


####################################################################################################################################
def test_build_module():
    """A module with nothing to shim generates the build and the test wrapper and nothing else."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)
        path_unit, file_list = _build(path, "common/error")

        assert_equal(file_list, ["meson.build", "meson_options.txt", "test.c"])

        # The options are copied as they are, since the build needs them to configure
        assert_equal(_read(path, path_unit, "meson_options.txt"), MESON_OPTIONS)

        # What was read from the repository, with the parts that are not built for testing commented out
        assert_equal(
            _read(path, path_unit, "meson.build"),
            MESON_READ
            + MESON_CONFIGURE
            + "add_global_arguments('-DDEBUG_COVERAGE', language : 'c')\n"
            + MESON_UNIT
            + MESON_EXECUTABLE
            + MESON_INCLUDE
            + "        lib_backtrace,\n"
            + MESON_LIB,
        )

        # The wrapper includes the module under test and the test module, and lists the runs it may run
        assert_equal(
            _read(path, path_unit, "test.c"),
            "/***/\n"
            "#define DEBUG_TEST_TRACE\n"
            '#include "../../repo/src/common/error.c"\n'
            '#define TEST_PATH "[TEST_PATH]/test-0"\n'
            '#define HRN_PATH "[TEST_PATH]/data-0"\n'
            '#define HRN_PATH_REPO "[REPO_PATH]"\n'
            '#define TEST_PROJECT_EXE "[TEST_PATH]/build/none/src/pgbackrest"\n'
            '#define TEST_PGB_PATH "../../../repo"\n'
            "#define TEST_LOG_EXPECT true\n"
            "#define LOG_LEVEL_TEST logLevelOff\n"
            "#define TEST_TIMING true\n"
            '#define TEST_ARCHITECTURE "x86_64"\n'
            "// No timezone specified\n"
            "#define TEST_SCALE 1\n"
            "#define TEST_CONTAINER false\n"
            '#define TEST_GROUP "pgbackrest"\n'
            "#define TEST_GROUP_ID 1001\n"
            '#define TEST_USER "pgbackrest"\n'
            "#define TEST_USER_LEN 10\n"
            "#define TEST_USER_ID 1000\n"
            '#define TEST_VM "none"\n'
            '#define TEST_PG_VERSION "invalid"\n'
            "#define TEST_IDX 0\n"
            '#include "../../repo/test/src/module/common/errorTest.c"\n'
            '#define TEST_PATH_BUILD "[TEST_PATH]/unit-0/none/build"\n'
            "\n"
            "void\n"
            "testRun(void)\n"
            "{\n"
            "    hrnAdd(  1,     true);\n"
            "    hrnAdd(  2,     true);\n"
            "}\n",
        )


####################################################################################################################################
def test_build_shim():
    """A module a harness shims is copied with the shimmed functions renamed, along with the harness that reaches into it."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)
        path_unit, file_list = _build(path, "common/exec")

        # The shim module keeps its normal path while the included module is written where the include resolves to it
        assert_equal(
            file_list,
            [
                "common/execInc.c.inc",
                "meson.build",
                "meson_options.txt",
                "src/common/exec.c",
                "test.c",
                "test/src/harness/config.c",
            ],
        )

        # An extern function is declared under the shimmed name so there is no missing prototype, a static function keeps a
        # declaration under its own name so the module can still call it, and the line count is unchanged so coverage still lines up
        assert_equal(
            _read(path, path_unit, "src/common/exec.c"),
            "/***/\n"
            '#include "common/execInc.c.inc"\n'
            "\n"
            "FN_EXTERN void execOne_SHIMMED(const char *command); FN_EXTERN void\n"
            "execOne_SHIMMED(const char *command)\n"
            "{\n"
            "    doExec();\n"
            "}\n"
            "\n"
            "static void execTwo(void); static void\n"
            "execTwo_SHIMMED(void)\n"
            "{\n"
            "    doTwo();\n"
            "}\n",
        )

        # A signature that runs over several lines is gathered onto one for the declaration, and a module holding more than one
        # shimmed function is written once with both of them renamed
        assert_equal(
            _read(path, path_unit, "common/execInc.c.inc"),
            "/***/\n"
            "FN_EXTERN void execThree_SHIMMED(const char *command, int flag); FN_EXTERN void\n"
            "execThree_SHIMMED(\n"
            "    const char *command,\n"
            "    int flag)\n"
            "{\n"
            "    doThree();\n"
            "}\n"
            "\n"
            "FN_EXTERN void execFour_SHIMMED(void); FN_EXTERN void\n"
            "execFour_SHIMMED(void)\n"
            "{\n"
            "    doFour();\n"
            "}\n",
        )

        # The harness includes the shimmed copy rather than the one in the repository, while a module it reaches into but shims no
        # function in comes straight from the repository
        assert_equal(
            _read(path, path_unit, "test/src/harness/config.c"),
            "/***/\n"
            '#include "[REPO_PATH]/src/common/execOther.c"\n'
            '#include "[TEST_PATH]/unit-0/none/src/common/exec.c"\n'
            "\n"
            "void\n"
            "hrnConfig(void)\n"
            "{\n"
            "}\n",
        )

        # The feature the module introduces is defined, the modules it depends on are compiled in, and the harness has its own
        # source compiled in. The harness itself is not, since the wrapper includes it directly.
        assert_equal(
            _read(path, path_unit, "meson.build"),
            MESON_READ
            + MESON_CONFIGURE
            + "add_global_arguments('-DHRN_INTEST_EXEC', language : 'c')\n"
            + "add_global_arguments('-DDEBUG_COVERAGE', language : 'c')\n"
            + MESON_UNIT
            + "    '../../repo/src/common/error.c',\n"
            + "    '../../repo/src/common/log.c',\n"
            + "    '../../repo/test/src/harness/config/helper.c',\n"
            + MESON_EXECUTABLE
            + MESON_INCLUDE
            + "        lib_backtrace,\n"
            + MESON_LIB,
        )

        # The wrapper includes the harness rather than the module, since the harness is what pulls the module in. Both covered
        # modules come from the one harness so it is included once.
        assert_in('#include "test/src/harness/config.c"\n', _read(path, path_unit, "test.c"))
        assert_equal(_read(path, path_unit, "test.c").count("test/src/harness/config.c"), 1)


####################################################################################################################################
def test_build_harness():
    """A harness a module does not reach into is compiled in, and a module a harness already includes is not compiled twice."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)
        path_unit, file_list = _build(path, "common/after")

        meson_build = _read(path, path_unit, "meson.build")

        # The feature an earlier module introduced is available here
        assert_in("add_global_arguments('-DHRN_FEATURE_EXEC', language : 'c')\n", meson_build)

        # The module this test depends on that a harness already includes is not compiled again
        assert_in("    '../../repo/src/common/error.c',\n", meson_build)
        assert_in("    '../../repo/src/common/log.c',\n", meson_build)
        assert_not_in("src/common/exec.c',\n", meson_build)

        # A harness that reaches into a module is compiled in when this module is not the one being tested, and one that reaches
        # into nothing is compiled straight from the repository
        assert_in("    'test/src/harness/config.c',\n", meson_build)
        assert_in("    '../../repo/test/src/harness/extra.c',\n", meson_build)


####################################################################################################################################
def test_build_integration():
    """An integration test leaves out the harnesses and shims that are only for unit tests."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)
        path_unit, file_list = _build(path, "real/all", vm="u22")

        # Nothing was shimmed and no harness needed a copy
        assert_equal(file_list, ["meson.build", "meson_options.txt", "test.c"])

        meson_build = _read(path, path_unit, "meson.build")

        assert_not_in("harness/config", meson_build)
        assert_in("    '../../repo/test/src/harness/extra.c',\n", meson_build)

        # An integration test builds outside a container even when one was named, so it is not flagged as being in one
        assert_not_in("TEST_CONTAINER_REQUIRED", meson_build)

        # The expect log is only kept for unit tests, and the vm is still the one the binary was built for
        test_c = _read(path, path_unit, "test.c")

        assert_in("#define TEST_LOG_EXPECT false\n", test_c)
        assert_in('#define TEST_VM "u22"\n', test_c)
        assert_in("#define TEST_CONTAINER false\n", test_c)


####################################################################################################################################
def test_build_option():
    """The options that change how the test is compiled."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)

        # A performance test is timed so it is optimized, and the debug trace it would otherwise carry is left out
        path_unit, file_list = _build(path, "performance/type")
        meson_build = _read(path, path_unit, "meson.build")

        assert_in("    c_args: [\n        '-O2',\n    ],\n", meson_build)
        assert_in("// Debug test trace not enabled\n", _read(path, path_unit, "test.c"))

        # A module that names a define has it set for the whole build
        assert_in("add_global_arguments('-DNDEBUG', language : 'c')\n", meson_build)

        # Profiling is optimized and linked without position independence, and the debug trace is left out for the same reason
        path_unit, file_list = _build(path, "common/error", profile=True, optimize=True)
        meson_build = _read(path, path_unit, "meson.build")

        assert_in("    c_args: [\n        '-O2',\n        '-pg',\n        '-no-pie',\n    ],\n", meson_build)
        assert_in("    link_args: [\n        '-pg',\n        '-no-pie',\n    ],\n", meson_build)
        assert_in("// Debug test trace not enabled\n", _read(path, path_unit, "test.c"))

        # Without back trace the library is not linked and the configuration that looks for it is commented out
        path_unit, file_list = _build(path, "common/error", back_trace=False)
        meson_build = _read(path, path_unit, "meson.build")

        assert_in("#    configuration.set('HAVE_LIBBACKTRACE'", meson_build)
        assert_not_in("lib_backtrace", meson_build)

        # Without coverage the define that turns it on is not set
        path_unit, file_list = _build(path, "common/error", coverage=False)

        assert_not_in("DEBUG_COVERAGE", _read(path, path_unit, "meson.build"))

        # In a container the tests that need one are enabled
        path_unit, file_list = _build(path, "common/error", vm="u22")

        assert_in("add_global_arguments('-DTEST_CONTAINER_REQUIRED', language : 'c')\n", _read(path, path_unit, "meson.build"))
        assert_in("#define TEST_CONTAINER true\n", _read(path, path_unit, "test.c"))


####################################################################################################################################
def test_build_option_test():
    """The options that change what the test runs rather than how it is compiled."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)

        # A single run can be selected while debugging, which leaves the rest listed but not run
        path_unit, file_list = _build(path, "common/error", test=2, tz="UTC", scale=4, log_timestamp=False)
        test_c = _read(path, path_unit, "test.c")

        assert_in("    hrnAdd(  1,    false);\n    hrnAdd(  2,     true);\n", test_c)

        # The timezone the test runs in, the scale of a performance test, and whether the log is timed
        assert_in('hrnTzSet("UTC");\n', test_c)
        assert_in("#define TEST_SCALE 4\n", test_c)
        assert_in("#define TEST_TIMING false\n", test_c)

        # The log level the test itself runs at
        path_unit, file_list = _build(path, "common/error", log_level_test=DETAIL)

        assert_in("#define LOG_LEVEL_TEST logLevelDetail\n", _read(path, path_unit, "test.c"))


####################################################################################################################################
def test_build_clean():
    """Anything left in the unit path that this test did not generate is removed, since the path is reused."""

    with tempfile.TemporaryDirectory() as path:
        _repo_create(path)

        # Build one module and then another in the same path
        _build(path, "common/exec")
        path_unit, file_list = _build(path, "common/error")

        # What the first test generated is gone
        assert_equal(file_list, ["meson.build", "meson_options.txt", "test.c"])

        # What the build path holds is left alone, since that is ninja's and rebuilding it is what the path is reused to avoid
        path_build = os.path.join(path_unit, "build")

        os.makedirs(path_build, exist_ok=True)

        with open(os.path.join(path_build, "build.ninja"), "w") as file:
            file.write("")

        path_unit, file_list = _build(path, "common/error")

        assert_equal(file_list, ["build/build.ninja", "meson.build", "meson_options.txt", "test.c"])


####################################################################################################################################
def test_build_path():
    """A code module is mapped to where it lives, and a path is expressed relative to another."""

    assert_equal(path_module("common/error"), "src/common/error")
    assert_equal(path_module("test/common/harnessLog"), "test/src/common/harnessLog")

    # Up out of the base path and back down into the compare path
    assert_equal(path_relative("/test/unit-0/none", "/test/repo"), "../../repo")
    assert_equal(path_relative("/test/unit-0/none", "/repo"), "../../../repo")
    assert_equal(path_relative("/test", "/test/repo"), "repo")

    # A path relative to itself is nothing, which is a mistake rather than an empty path
    with assert_raises(ToolError) as error:
        path_relative("/test/repo", "/test/repo")

    assert_equal(str(error.exception), "base and compare paths may not be equal")


####################################################################################################################################
def test_build_shim_error():
    """Source the shim cannot make sense of is an error rather than a file that will not compile."""

    # The shim walks back to the line above the function, so a function cannot be the first thing in the file
    with assert_raises(ToolError) as error:
        build_shim("execOne(void)\n{\n}\n", ["execOne"])

    assert_equal(str(error.exception), "shimmed function may not be on the first line")

    # The shim reads to the opening brace, which a properly formatted C file always has
    with assert_raises(ToolError) as error:
        build_shim("void\nexecOne(\n    void)\n", ["execOne"])

    assert_equal(str(error.exception), "unable to find the end of the function signature")

    # A file that does not end with a linefeed would lose its last line
    with assert_raises(ToolError) as error:
        build_shim("void\nexecOne(void)\n{\n}", ["execOne"])

    assert_equal(str(error.exception), "shim module must end with a linefeed")

    # A name that only looks like the function is left alone, i.e. one that is longer or is not where a definition would be
    assert_equal(build_shim("void\nexecOneMore(void)\n{\n}\n", ["execOne"]), "void\nexecOneMore(void)\n{\n}\n")
    assert_equal(build_shim("void\n    execOne(void)\n{\n}\n", ["execOne"]), "void\n    execOne(void)\n{\n}\n")


####################################################################################################################################
def test_build_user():
    """The user and group the test runs as are looked up once, since they are used several times and never change."""

    # Whoever the tests run as, which is only known here by asking twice and getting the same answer
    assert_equal(user_name(), user_name())
    assert_equal(group_name(), group_name())

    assert_true(len(user_name()) > 0)
    assert_true(len(group_name()) > 0)
