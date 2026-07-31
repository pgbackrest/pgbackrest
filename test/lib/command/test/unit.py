"""Unit Command.

Prepares the unit test for one test module. A C module is generated into the unit path and compiled there, and the caller runs the
resulting binary so it can wrap it in valgrind. A python module has nothing to build so it is run here.

The unit path is reused across runs so a rebuild only compiles what changed; a failed build is retried once from a clean path since
a stale build directory is the usual cause."""

####################################################################################################################################
import os
import shutil
import sys

from command.test.build import TestBuild
from command.test.define import (
    TEST_LANG_PYTHON,
    TEST_LIB_LIST,
    TEST_TYPE_PERFORMANCE,
    test_def_file,
    test_def_find,
    test_def_parse,
    test_lib_path,
    test_lib_split,
)
from common.error import ToolError, check
from common.exec import exec_one
from common.log import *
from common.storage import file_remove, path_create, path_list
from common.vm import *


####################################################################################################################################
def _path_recreate(path):
    """Empty a path, resetting permissions first if needed, and recreate it."""

    try:
        shutil.rmtree(path)
    except FileNotFoundError:
        pass
    except OSError:
        # A test may leave paths that cannot be removed until the mode is reset
        exec_one("chmod -R 777 '%s'" % path)
        shutil.rmtree(path)

    path_create(path, mode=0o770)


####################################################################################################################################
def _python_module(name):
    """Convert a code module name to the module it is imported as, e.g. test/common/string_id becomes common.string_id."""

    return test_lib_split(name)[1].replace("/", ".")


####################################################################################################################################
def _test_python(config, module):
    """Run a python test module.

    The test file lives beside the C test modules and is named for the test module, e.g. test/common/string is
    test/src/module/test/common/string_test.py. A test file may cover more than one code module.

    It runs in a separate interpreter so the modules it declared are the only library modules it can import. Running it here would
    not work since the harness has already imported most of the library and an import found in sys.modules never reaches a hook."""

    path_test = os.path.join(config.repo_path, test_def_file(module))

    if not os.path.isfile(path_test):
        raise ToolError("unable to find test module '%s'" % path_test)

    # What the test may import: the code modules it covers, plus what it declared, plus what earlier tests covered
    allow_list = [_python_module(coverage.name) for coverage in module.coverage_list if coverage.coverable]
    allow_list += [_python_module(name) for name in list(module.depend_list) + list(module.include_list)]

    # Libraries the test may import from, which is the one the test module lives in and the ones below it in the hierarchy. A test
    # of the build library cannot reach the test library this way, the same as the build tool itself cannot.
    path_lib_list = [os.path.join(config.repo_path, test_lib_path(lib)) for lib in TEST_LIB_LIST[test_lib_split(module.name)[0]]]

    # A single test can be run while debugging with --test-name, e.g.
    # test.py unit test/common/string --test-name=test_string_id_render
    command = "%s '%s/command/test/python.py' --lib='%s' --test='%s' --allow='%s' --name='%s'" % (
        sys.executable,
        os.path.join(config.repo_path, test_lib_path("test")),
        ",".join(path_lib_list),
        path_test,
        ",".join(allow_list),
        config.test_name or "",
    )

    # Coverage is written where the caller asks for it, since the harness runs from a copy of the repository and the report is
    # built from the original
    if config.coverage and config.coverage_file is not None:
        path_create(os.path.dirname(config.coverage_file))
        command += " --coverage='%s'" % config.coverage_file

    # Write the output as it came back rather than through the log, which would indent it as a continuation
    print(exec_one(command).rstrip())


####################################################################################################################################
def cmd_unit(config):
    """Prepare the unit test for a test module."""

    # Find test
    module = test_def_find(test_def_parse(config.repo_path), config.module)

    # A python module has nothing to build so run it here
    if module.lang == TEST_LANG_PYTHON:
        _test_python(config, module)

        return

    # Get test architecture
    architecture = config.vm_arch if config.vm_arch is not None else host_arch()

    test_build = TestBuild(config, module, architecture)
    path_unit = test_build.path_unit
    path_unit_build = test_build.path_unit_build

    # Meson setup depends on the build type, which is release when the module sets a flag, is a performance test, or is profiled
    meson_setup = "-Dbuildtype="

    if module.flag is not None or config.profile or module.type == TEST_TYPE_PERFORMANCE:
        if module.flag is not None and module.flag != "-DNDEBUG":
            raise ToolError("unexpected define '%s'" % module.flag)

        meson_setup += "release"
    else:
        meson_setup += "debug"

    meson_setup += " -Db_coverage=%s" % ("true" if config.coverage else "false")

    build_retry = False

    while True:
        try:
            # Generate the unit build
            test_build.build()

            if not os.path.exists(os.path.join(path_unit_build, "build.ninja")):
                log(DETAIL, "meson setup")

                exec_one("meson setup -Dwerror=true -Dfatal-errors=true %s '%s' '%s'" % (meson_setup, path_unit_build, path_unit))
            # Else reconfigure
            else:
                log(DETAIL, "meson configure")

                exec_one("meson configure %s '%s'" % (meson_setup, path_unit_build))

            # Remove old coverage data
            path_coverage = os.path.join(path_unit_build, "test-unit.p")

            for name in path_list(path_coverage, expression=r"\.gcda$"):
                file_remove(os.path.join(path_coverage, name))

            # Remove old profile data
            file_remove(os.path.join(path_unit_build, "gmon.out"))

            # Ninja build
            exec_one("ninja -C '%s'" % path_unit_build)

            break
        except ToolError as error:
            # If this is the first build failure then clean the build path and retry
            if build_retry:
                raise ToolError("build failed for unit %s: %s" % (config.module, error))

            build_retry = True

            log(WARN, "build failed for unit %s -- will retry: %s" % (config.module, error))
            _path_recreate(path_unit)
