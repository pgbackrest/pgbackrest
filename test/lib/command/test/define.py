"""Parse Define Yaml.

Keys are looked up by name so only the sequences carry order, which is what YAML guarantees.

Definitions accumulate as the file is read: a test module inherits the coverage, harnesses, shims, and features declared by every
module before it, which is why the module list is built in a single pass over the types in the order they are declared."""

####################################################################################################################################
import os

import yaml

from common.error import ToolError, check
from common.render import bld_enum

# Path the test modules live in, relative to the repository. Everything in it is a test module, which is what lets the linter
# report a module that was never declared here.
TEST_MODULE_PATH = "test/src/module/"

# Test types, in the order they are declared in define.yaml and accumulated
TEST_TYPE_UNIT = "unit"
TEST_TYPE_INTEGRATION = "integration"
TEST_TYPE_PERFORMANCE = "performance"
TEST_TYPE_TOOL = "tool"

TEST_TYPE_LIST = (TEST_TYPE_UNIT, TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE, TEST_TYPE_TOOL)

# Language a test module is written in, which follows from its type since a tool is python and everything else is C. A C test is
# generated and compiled into a binary while a python test is run directly.
TEST_LANG_C = "c"
TEST_LANG_PYTHON = "python"

TEST_LANG_LIST = (TEST_LANG_C, TEST_LANG_PYTHON)

# Libraries a python module may import from, keyed by the library it lives in, which is the first component of its module name. A
# tool sees its own library and the ones below it in the hierarchy, listed in search order, which is what include_directories does
# for the C.
TEST_LIB_LIST = {
    "build": ("build",),
    "doc": ("doc", "build"),
    "test": ("test", "doc", "build"),
}


####################################################################################################################################
def test_lib_split(name):
    """Split a python module name into the library it lives in and the module within it.

    For example "build/common/render" becomes ("build", "common/render"), where the module is imported as common.render from the
    build library."""

    lib, _, module = name.partition("/")

    check(lib in TEST_LIB_LIST, "python module '%s' must be in one of these libraries: %s" % (name, ", ".join(TEST_LIB_LIST)))

    return lib, module


####################################################################################################################################
def test_lib_path(lib):
    """Path of a library relative to the repository, e.g. "build" becomes "build/lib"."""

    return "%s/lib" % lib


####################################################################################################################################
class TestDefCoverage:
    """A code module covered by a test module."""

    def __init__(self, name, coverable, included):
        self.name = name
        self.coverable = coverable  # Does this code module include coverable code?
        self.included = included  # Is this module included in another module?


####################################################################################################################################
class TestDefShimFunction:
    """A function that the harness shims."""

    def __init__(self, name, inc):
        self.name = name
        self.inc = inc  # Included module the function is defined in, or None when it is in the shim module


####################################################################################################################################
class TestDefShim:
    """A code module that the harness shims."""

    def __init__(self, name, integration, function_list):
        self.name = name
        self.integration = integration  # Include in integration tests?
        self.function_list = function_list


####################################################################################################################################
class TestDefHarness:
    """A harness module."""

    def __init__(self, name, integration, include_list):
        self.name = name
        self.integration = integration  # Include in integration tests?
        self.include_list = include_list  # Modules included directly in the harness, in the order they are included


####################################################################################################################################
class TestDefModule:
    """A test module."""

    def __init__(self, name, type):
        self.name = name
        self.type = type
        self.lang = TEST_LANG_C  # Language the test module is written in
        self.vm_list = []  # Vms the test runs on, empty for all of them
        self.total = 0  # Total sub-tests
        self.pg_required = False  # Is PostgreSQL required?
        self.bin_required = False  # Is the pgbackrest binary required?
        self.container_required = False  # Is a container required?
        self.flag = None  # Compilation flags
        self.feature = None  # Feature this module introduces
        self.feature_list = []  # Features available to this module
        self.coverage_list = []  # Code modules covered by this test module
        self.depend_list = []  # Code modules this test module depends on
        self.include_list = []  # Additional code modules to include in the test module
        self.harness_list = []  # Harnesses used by this test
        self.shim_list = []  # Shims used by this test

    ################################################################################################################################
    def coverage_find(self, name):
        """Find a coverage entry by code module name, or None when the module is not covered."""

        for coverage in self.coverage_list:
            if coverage.name == name:
                return coverage

        return None

    ################################################################################################################################
    def shim_find(self, name):
        """Find a shim by code module name, or None when the module is not shimmed."""

        for shim in self.shim_list:
            if shim.name == name:
                return shim

        return None


####################################################################################################################################
def _parse_shim(shim, harness_include_list, shim_list):
    """Parse a single shim entry, either a bare module name or a map with a name and a function list."""

    # A bare scalar is a module that is shimmed but has no shimmed functions
    if isinstance(shim, str):
        harness_include_list.append(shim)

        return

    function_list = []

    for key, value in shim.items():
        if key == "name":
            continue

        check(key == "function", "invalid key '%s'" % key)

        for function in value:
            # A bare scalar is a function defined in the shim module, else a map naming the included module it is defined in
            if isinstance(function, str):
                function_list.append(TestDefShimFunction(function, None))
            else:
                for name, detail in function.items():
                    function_list.append(TestDefShimFunction(name, detail["inc"]))

    check("name" in shim, "shim name is required")

    harness_include_list.append(shim["name"])
    shim_list.append(TestDefShim(shim["name"], True, function_list))


####################################################################################################################################
def _parse_harness(harness, global_harness_list, global_shim_list):
    """Parse a single harness entry, either a bare name or a map with a name, integration flag, and shims."""

    if isinstance(harness, str):
        global_harness_list.append(TestDefHarness(harness, True, []))

        return

    include_list = []
    shim_list = []

    for key, value in harness.items():
        if key in ("name", "integration"):
            continue

        check(key == "shim", "invalid key '%s'" % key)

        # Shim modules are included in the harness in the order listed
        for shim in value:
            _parse_shim(shim, include_list, shim_list)

    integration = harness.get("integration", True)

    # Apply the harness integration flag to its shims now that the whole harness has been parsed
    for shim in shim_list:
        shim.integration = integration
        global_shim_list.append(shim)

    global_harness_list.append(TestDefHarness(harness["name"], integration, include_list))


####################################################################################################################################
def _parse_coverage(coverage_list_raw):
    """Parse the coverage list, which holds either a bare code module name or a map tagging it noCode or included."""

    result = []

    for coverage in coverage_list_raw:
        if isinstance(coverage, str):
            entry = TestDefCoverage(coverage, True, False)
        else:
            for name, type in coverage.items():
                check(type in ("included", "noCode"), "invalid coverage type %s" % type)

                entry = TestDefCoverage(name, type == "included", True)

        result.append(entry)

    return result


####################################################################################################################################
# Parse a single test module
# Options that may be set for a path, which every module under it inherits and may override, by the type they are valid for. Only
# an integration module runs against multiple PostgreSQL versions, and only a tool is limited to certain vms since it is the same
# everywhere while a C test is exercising the platform it runs on.
_OPTION_KEY_LIST = {TEST_TYPE_INTEGRATION: ("db",), TEST_TYPE_TOOL: ("vm",)}

# Keys that define a test. An entry that has none of them sets options for the path it names rather than defining a module.
_TEST_KEY_LIST = ("binReq", "containerReq", "coverage", "define", "depend", "feature", "harness", "include", "total")

# Keys a module may define. Coverage is applied before depend so the depend list is built in a fixed order no matter how the keys
# are written, since a mapping does not carry order.
_MODULE_KEY_LIST = ("name",) + _TEST_KEY_LIST


####################################################################################################################################
def _option_get(option_list, module, key, default):
    """Value of an option for a module.

    What the module sets wins, else the innermost path that sets it, which is the last one declared since a path is declared before
    everything under it."""

    if key in module:
        return module[key]

    for option in reversed(option_list):
        if module["name"].startswith(option["name"] + "/") and key in option:
            return option[key]

    return default


####################################################################################################################################
def _parse_module(module, type, option_list, state):
    """Parse a single test module."""

    result = TestDefModule(module["name"], type)

    # A tool is written in python and runs on the vms it declares, everything else is C and runs on all of them
    result.lang = TEST_LANG_PYTHON if type == TEST_TYPE_TOOL else TEST_LANG_C
    result.vm_list = list(_option_get(option_list, module, "vm", []))
    result.pg_required = _option_get(option_list, module, "db", False)
    result.total = module.get("total", 0)
    result.bin_required = module.get("binReq", False)
    result.container_required = module.get("containerReq", False)
    result.flag = module.get("define")
    result.feature = module.get("feature")
    result.include_list = list(module.get("include", []))

    # Each language accumulates its own dependencies. A module may use what it declares plus everything covered by the modules
    # before it, which is what makes the module order in this file a documented hierarchy rather than a formality.
    depend_state = state["depend"][result.lang]

    if "coverage" in module:
        result.coverage_list = _parse_coverage(module["coverage"])

        for coverage in result.coverage_list:
            if coverage.coverable and not coverage.included and coverage.name not in depend_state:
                depend_state.append(coverage.name)

    for depend in module.get("depend", []):
        if depend not in depend_state:
            depend_state.append(depend)

    if "harness" in module:
        harness = module["harness"]

        # Harness may be a single entry (bare name or map) or a sequence of entries
        for entry in harness if isinstance(harness, list) else [harness]:
            _parse_harness(entry, state["harness"], state["shim"])

    # The depend list is the accumulated list minus anything this module already covers or includes
    result.depend_list = [
        depend for depend in depend_state if result.coverage_find(depend) is None and depend not in result.include_list
    ]

    # Harnesses and shims accumulate across modules, so this module gets everything declared so far
    result.harness_list = [TestDefHarness(h.name, h.integration, list(h.include_list)) for h in state["harness"]]
    result.shim_list = [TestDefShim(s.name, s.integration, list(s.function_list)) for s in state["shim"]]
    result.feature_list = list(state["feature"])

    if result.feature is not None:
        result.feature = result.feature.upper()
        state["feature"].append(result.feature)

    return result


####################################################################################################################################
def test_def_parse(path_repo):
    """Parse define.yaml into the list of test modules."""

    path_define = os.path.join(path_repo, "test/define.yaml")

    with open(path_define, "r") as file:
        define = yaml.safe_load(file)

    result = []

    # Lists that accumulate across every module in the file, in declaration order. Dependencies accumulate per language since a C
    # test cannot compile a python module and a python test cannot import a C one.
    state = {"depend": {lang: [] for lang in TEST_LANG_LIST}, "feature": [], "harness": [], "shim": []}

    for type in TEST_TYPE_LIST:
        # Options declared for a path, which only apply to the type they are declared in
        option_list = []

        # Everything named so far in this type, which is what the options are checked against
        name_list = []

        # Keys valid in this type, which is every module key plus the options the type has
        key_list = _MODULE_KEY_LIST + _OPTION_KEY_LIST.get(type, ())

        for module in define[type]:
            name = module.get("name")

            check(name is not None, "module name is required")

            for key in module:
                check(key in key_list, "unexpected keyword '%s' in module '%s'" % (key, name))

            # An entry that defines no test sets options for everything under it. Requiring it to come first means the options a
            # module has are always the ones above it in the file.
            if not any(key in module for key in _TEST_KEY_LIST):
                check(
                    not any(prior == name or prior.startswith(name + "/") for prior in name_list),
                    "options for '%s' must be declared before the modules under it" % name,
                )

                option_list.append(module)
            else:
                result.append(_parse_module(module, type, option_list, state))

            name_list.append(name)

    return result


####################################################################################################################################
def test_def_file(module):
    """File a test module lives in, relative to the repository.

    A C module is named in camel case, e.g. common/stack-trace is test/src/module/common/stackTraceTest.c, while a python module is
    named exactly as it is declared, e.g. test/common/vm is test/src/module/test/common/vm_test.py."""

    if module.lang == TEST_LANG_PYTHON:
        return "%s%s_test.py" % (TEST_MODULE_PATH, module.name)

    return "%s%sTest.c" % (TEST_MODULE_PATH, bld_enum(None, module.name))


####################################################################################################################################
def test_def_find(module_list, name):
    """Find a test module by name."""

    for module in module_list:
        if module.name == name:
            return module

    raise ToolError("'%s' is not a valid module" % name)
