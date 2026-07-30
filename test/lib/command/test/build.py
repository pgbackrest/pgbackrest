"""Test Build Handler.

Generates everything the unit build needs into the unit path: the shimmed copies of the modules the harness needs to reach into, the
harnesses that include them, meson.build, and test.c.

Files are only written when the content changes so an unchanged file keeps its timestamp and ninja does not rebuild the world.
Anything left in the unit path that this test did not generate is removed, since the path is reused across tests."""

####################################################################################################################################
import os

from command.test.define import TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE, TEST_TYPE_UNIT
from common.error import check
from common.log import *
from common.render import bld_enum
from common.storage import file_read, file_remove, file_write_differs, path_list, path_list_recurse
from common.user import group_id, group_name, user_id, user_name

# Comment block used to divide the generated meson.build, matching the width used in the rest of the project
MESON_COMMENT_BLOCK = "#" * 132


####################################################################################################################################
def path_module(module_name):
    """Map a code module name to its path in the repository, e.g. "common/error/error" becomes "src/common/error/error"."""

    if module_name.startswith("test/"):
        return "test/src" + module_name[len("test") :]

    return "src/" + module_name


####################################################################################################################################
def path_relative(base, compare):
    """Generate a relative path from the base path to the compare path."""

    check(base != compare, "base and compare paths may not be equal")

    base_list = base.split("/")
    compare_list = compare.split("/")
    index = 0

    # Find the part of the paths that is the same
    while index < len(base_list) and index < len(compare_list) and base_list[index] == compare_list[index]:
        index += 1

    return "/".join([".."] * (len(base_list) - index) + compare_list[index:])


####################################################################################################################################
def build_shim(shim_c, function_list):
    """Rewrite the shimmed functions in a module so the harness can wrap them.

    The function definition is renamed with a _SHIMMED suffix and a forward declaration is added ahead of it. A static function
    keeps a declaration under its original name so the module can still call it, while an extern function is declared under the
    shimmed name so there is no missing-prototypes warning."""

    result = ""
    in_list = shim_c.split("\n")

    check(in_list[-1] == "", "shim module must end with a linefeed")

    for in_idx, line in enumerate(in_list):
        found = False

        for function in function_list:
            if not line.startswith(function) or line.find("(") != len(function):
                continue

            check(in_idx > 0, "shimmed function may not be on the first line")
            found = True

            # Add the forward declaration, which runs from the function name to the line before the opening brace
            result += " "

            if in_list[in_idx - 1].startswith("static "):
                result += line
            else:
                result += function + "_SHIMMED" + line[len(function) :]

            scan_idx = in_idx + 1

            while True:
                # In a properly formatted C file the end of the list can never be reached
                check(scan_idx < len(in_list), "unable to find the end of the function signature")

                if in_list[scan_idx] == "{":
                    break

                if in_list[scan_idx - 1].endswith(","):
                    result += " "

                result += in_list[scan_idx].strip()
                scan_idx += 1

            result += "; " + in_list[in_idx - 1]

            # Alter the function name so it can be shimmed
            result += "\n" + function + "_SHIMMED" + line[len(function) :]
            break

        # Just copy the line when a function is not found
        if not found:
            if in_idx != 0:
                result += "\n"

            result += line

    return result


####################################################################################################################################
class TestBuild:
    """Build the unit test into the unit path."""

    def __init__(self, config, module, architecture):
        self.path_repo = config.repo_path
        self.path_test = config.test_path
        self.vm = "none" if module.type == TEST_TYPE_INTEGRATION else config.vm
        self.vm_int = config.vm  # Vm used for integration and the pgbackrest binary
        self.vm_id = config.vm_id
        self.pg_version = config.pg_version
        self.module = module
        self.test = config.test
        self.scale = config.scale
        self.log_level = config.log_level_test
        self.log_time = config.log_timestamp
        self.time_zone = config.tz
        self.architecture = architecture
        self.coverage = config.coverage
        self.profile = config.profile
        self.optimize = config.optimize
        self.back_trace = config.back_trace

        self.path_unit = os.path.join(self.path_test, "unit-%u" % self.vm_id, self.vm)
        self.path_unit_build = os.path.join(self.path_unit, "build")

        # Files generated into the unit path, so anything else there can be removed
        self._file_list = []

    ################################################################################################################################
    def _write(self, file, content):
        """Write a file into the unit path and record that it belongs to this test."""

        file_write_differs(os.path.join(self.path_unit, file), content)
        self._file_list.append(file)

    ################################################################################################################################
    def _read_repo(self, file):
        """Read a file from the repository."""

        return file_read(os.path.join(self.path_repo, file))

    ################################################################################################################################
    def _build_shim(self):
        """Write the shimmed copy of every module a harness shims."""

        for shim in self.module.shim_list:
            # Skip this shim for integration tests
            if self.module.type == TEST_TYPE_INTEGRATION and not shim.integration:
                continue

            # The shim module .c is always shimmed and written since the harness includes it. A function may instead be defined in
            # an included .c.inc module, in which case that module is shimmed as well.
            shim_module_list = [shim.name]

            for function in shim.function_list:
                if function.inc is not None:
                    module = os.path.dirname(shim.name) + "/" + function.inc

                    if module not in shim_module_list:
                        shim_module_list.append(module)

            for shim_module_idx, shim_module in enumerate(shim_module_list):
                # The first module is the shim module .c; the rest are included .c.inc modules
                included = shim_module_idx != 0

                # Get the functions to shim in this module
                function_list = []

                for function in shim.function_list:
                    function_module = shim.name if function.inc is None else os.path.dirname(shim.name) + "/" + function.inc

                    if function_module == shim_module:
                        function_list.append(function.name)

                # Read the source. An included module is a .c.inc, otherwise a .c.
                read_file = path_module(shim_module) + ".c" + (".inc" if included else "")

                # Write the shimmed source. The shim module .c keeps its normal path. An included .c.inc uses the include path so it
                # resolves to this copy (via the unit include dir, before the repo copy) when the shim module includes it.
                write_file = shim_module + ".c.inc" if included else read_file

                self._write(write_file, build_shim(self._read_repo(read_file), function_list))

    ################################################################################################################################
    def _build_harness(self):
        """Write the harnesses that include shimmed modules.

        Returns the harnesses that apply to this test paired with the source to compile for each, and every module they include. A
        harness with includes is copied into the unit path with the includes filled in, otherwise it is compiled from the
        repository."""

        harness_list = []
        harness_include_list = []
        path_repo_rel = path_relative(self.path_unit, self.path_repo)

        for harness in self.module.harness_list:
            # Skip this harness for integration tests
            if self.module.type == TEST_TYPE_INTEGRATION and not harness.integration:
                continue

            harness_file = "test/src/harness/%s.c" % harness.name

            # If there are includes then copy and update the harness
            if harness.include_list:
                include_replace = []

                for include in harness.include_list:
                    # A shimmed module is included from the unit path, everything else straight from the repository
                    path = self.path_unit if self.module.shim_find(include) is not None else self.path_repo
                    include_replace.append('#include "%s/%s.c"' % (path, path_module(include)))
                    harness_include_list.append(include)

                harness_c = self._read_repo(harness_file).replace("{[SHIM_MODULE]}", "\n".join(include_replace))

                self._write(harness_file, harness_c)
                harness_list.append((harness, harness_file))
            # Else harness can be referenced directly from the repo path
            else:
                harness_list.append((harness, "%s/%s" % (path_repo_rel, harness_file)))

        return harness_list, harness_include_list

    ################################################################################################################################
    def _build_meson(self, harness_list, harness_include_list):
        """Write meson.build for the unit."""

        module = self.module
        path_repo_rel = path_relative(self.path_unit, self.path_repo)

        self._write("meson_options.txt", self._read_repo("meson_options.txt"))

        meson_build = self._read_repo("meson.build")

        # Comment out subdirs that are not used for testing
        meson_build = meson_build.replace("subdir('", "# subdir('")

        # Comment out the distribution script, which is only used when building a distribution tarball
        meson_build = meson_build.replace("meson.add_dist_script(", "# meson.add_dist_script(")

        if not self.back_trace:
            meson_build = meson_build.replace(
                "    configuration.set('HAVE_LIBBACKTRACE'", "#    configuration.set('HAVE_LIBBACKTRACE'"
            )

        # Write build.auto.in
        meson_build += (
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

        # Configure features
        if module.feature is not None:
            meson_build += "add_global_arguments('-DHRN_INTEST_%s', language : 'c')\n" % module.feature

        for feature in module.feature_list:
            meson_build += "add_global_arguments('-DHRN_FEATURE_%s', language : 'c')\n" % feature

        # Add compiler flags
        if module.flag is not None:
            meson_build += "add_global_arguments('%s', language : 'c')\n" % module.flag

        # Add coverage
        if self.coverage:
            meson_build += "add_global_arguments('-DDEBUG_COVERAGE', language : 'c')\n"

        # Add container flag
        if self.vm != "none":
            meson_build += "add_global_arguments('-DTEST_CONTAINER_REQUIRED', language : 'c')\n"

        # Build unit test
        meson_build += "\n" + MESON_COMMENT_BLOCK + "\n# Unit test\n" + MESON_COMMENT_BLOCK + "\n" + "src_unit = files(\n"

        for depend in module.depend_list:
            if depend in harness_include_list:
                continue

            meson_build += "    '%s/%s.c',\n" % (path_repo_rel, path_module(depend))

        # Add harnesses
        for harness, harness_file in harness_list:
            # Add harness depends
            harness_depend_path = "test/src/harness/%s" % harness.name

            for name in path_list(os.path.join(self.path_repo, harness_depend_path), expression=r"\.c$"):
                meson_build += "    '%s/%s/%s',\n" % (path_repo_rel, harness_depend_path, name)

            # Skip the harness when one of its includes is covered or included by the module, since test.c includes it directly
            if any(module.coverage_find(include) is not None or include in module.include_list for include in harness.include_list):
                continue

            meson_build += "    '%s',\n" % harness_file

        meson_build += (
            "    '%s/test/src/harness/test.c',\n" % path_repo_rel
            + "    'test.c',\n"
            + ")\n"
            + "\n"
            + "executable(\n"
            + "    'test-unit',\n"
            + "    sources: src_unit,\n"
        )

        # Add C args
        c_arg = ""

        if self.optimize or module.type == TEST_TYPE_PERFORMANCE:
            c_arg += "\n        '-O2',"

        if self.profile:
            c_arg += "\n        '-pg',\n        '-no-pie',"

        if c_arg:
            meson_build += "    c_args: [%s\n    ],\n" % c_arg

        # Add linker args
        link_arg = ""

        if self.profile:
            link_arg += "\n        '-pg',\n        '-no-pie',"

        if link_arg:
            meson_build += "    link_args: [%s\n    ],\n" % link_arg

        meson_build += (
            "    include_directories:\n"
            + "        include_directories(\n"
            + "            '.',\n"
            + "            '%s/src',\n" % path_repo_rel
            + "            '%s/test/src',\n" % path_repo_rel
            + "        ),\n"
            + "    dependencies: [\n"
        )

        if self.back_trace:
            meson_build += "        lib_backtrace,\n"

        meson_build += (
            "        lib_bz2,\n"
            + "        lib_openssl,\n"
            + "        lib_lz4,\n"
            + "        lib_pq,\n"
            + "        lib_ssh2,\n"
            + "        lib_xml,\n"
            + "        lib_z,\n"
            + "        lib_zstd,\n"
            + "    ],\n"
            + ")\n"
        )

        self._write("meson.build", meson_build)

    ################################################################################################################################
    def _build_test_c(self, harness_list):
        """Write test.c, the wrapper that includes the code under test and the test module itself."""

        module = self.module
        path_repo_rel = path_relative(self.path_unit, self.path_repo)
        test_c = self._read_repo("test/src/test.c")

        # Enable debug test trace
        if not self.profile and module.type != TEST_TYPE_PERFORMANCE:
            test_c = test_c.replace("{[C_TEST_DEBUG_TEST_TRACE]}", "#define DEBUG_TEST_TRACE")
        else:
            test_c = test_c.replace("{[C_TEST_DEBUG_TEST_TRACE]}", "// Debug test trace not enabled")

        # Files to test/include. A file covered by a harness is pulled in by including that harness instead.
        test_include_file_list = [
            coverage.name for coverage in module.coverage_list if coverage.coverable and not coverage.included
        ] + list(module.include_list)

        test_include_file = []
        harness_included = []

        for include in test_include_file_list:
            # A module pulled in by a harness is covered by including that harness rather than the module itself
            harness_file = next((file for harness, file in harness_list if include in harness.include_list), None)

            if harness_file is not None:
                if harness_file not in harness_included:
                    test_include_file.append('#include "%s"' % harness_file)
                    harness_included.append(harness_file)
            else:
                test_include_file.append('#include "%s/%s.c"' % (path_repo_rel, path_module(include)))

        replace = {
            "{[C_INCLUDE]}": "\n".join(test_include_file),
            # Test path
            "{[C_TEST_PATH]}": os.path.join(self.path_test, "test-%u" % self.vm_id),
            # Harness data path
            "{[C_HRN_PATH]}": os.path.join(self.path_test, "data-%u" % self.vm_id),
            # Harness repo path
            "{[C_HRN_PATH_REPO]}": self.path_repo,
            # Path to the project exe when it exists
            "{[C_TEST_PROJECT_EXE]}": os.path.join(self.path_test, "build", self.vm_int, "src/pgbackrest"),
            # Path to source -- used to construct __FILENAME__ tests
            "{[C_TEST_PGB_PATH]}": "../" + path_repo_rel,
            # Test expect logging
            "{[C_TEST_LOG_EXPECT]}": "true" if module.type == TEST_TYPE_UNIT else "false",
            # Test log level
            "{[C_LOG_LEVEL_TEST]}": log_level_enum(self.log_level),
            # Log time/timestamp
            "{[C_TEST_TIMING]}": "true" if self.log_time else "false",
            # Test architecture
            "{[C_TEST_ARCHITECTURE]}": self.architecture,
            # Test timezone
            "{[C_TEST_TZ]}": ("// No timezone specified" if self.time_zone is None else 'hrnTzSet("%s");' % self.time_zone),
            # Scale performance test
            "{[C_TEST_SCALE]}": "%u" % self.scale,
            # Does this test run in a container?
            "{[C_TEST_CONTAINER]}": "true" if self.vm != "none" else "false",
            # User/group info
            "{[C_TEST_GROUP]}": group_name(),
            "{[C_TEST_GROUP_ID]}": "%u" % group_id(),
            "{[C_TEST_USER]}": user_name(),
            "{[C_TEST_USER_LEN]}": "%u" % len(user_name()),
            "{[C_TEST_USER_ID]}": "%u" % user_id(),
            # VM for integration testing
            "{[C_TEST_VM]}": self.vm_int,
            # PostgreSQL version for integration testing
            "{[C_TEST_PG_VERSION]}": self.pg_version,
            # Test id
            "{[C_TEST_IDX]}": "%u" % self.vm_id,
            # Include test file
            "{[C_TEST_INCLUDE]}": '#include "%s/test/src/module/%sTest.c"' % (path_repo_rel, bld_enum(None, module.name)),
            # Test list
            "{[C_TEST_LIST]}": "\n    ".join(
                "hrnAdd(%3u, %8s);" % (idx + 1, "true" if self.test in (None, idx + 1) else "false") for idx in range(module.total)
            ),
            # Profiling
            "{[C_TEST_PROFILE]}": "true" if self.profile else "false",
            "{[C_TEST_PATH_BUILD]}": self.path_unit_build,
        }

        for key, value in replace.items():
            test_c = test_c.replace(key, value)

        self._write("test.c", test_c)

    ################################################################################################################################
    def build(self):
        """Generate the unit build."""

        self._file_list = []

        self._build_shim()
        harness_list, harness_include_list = self._build_harness()
        self._build_meson(harness_list, harness_include_list)
        self._build_test_c(harness_list)

        # Clean files that are not valid for this test
        for name in path_list_recurse(self.path_unit):
            if name.startswith("build/") or name in self._file_list:
                continue

            file_remove(os.path.join(self.path_unit, name), error_on_missing=True)
