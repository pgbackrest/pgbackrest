"""Test Parse Define Yaml."""

####################################################################################################################################
import os
import tempfile

from harness.test import *

from command.test.define import *
from common.error import *

# A define file that exercises every shape the parser accepts. It is deliberately small since what matters here is the shapes rather
# than the test list, which is what the real file provides.
DEFINE = """
unit:
  - name: common

    test:
      - name: error
        total: 2

        coverage:
          - common/error
          - common/errorInternal: noCode
          - common/errorType: included

      - name: stack-trace
        total: 1
        binReq: true
        containerReq: true
        define: -DDEBUG_TEST_TRACE
        feature: stack-trace
        harness: error

        include:
          - common/stackTrace

        depend:
          - common/error
          - common/type/string

      - name: type-string
        total: 3
        vm: [none, u22]

        include:
          - common/error

        coverage:
          - common/type/string

        harness:
          - name: config
            integration: false

            shim:
              - common/config
              - name: common/exec
                function:
                  - execOne
                  - execTwo:
                      inc: common/execInc

  - name: test
    lang: python
    vm: [rh9]

    test:
      - name: common/error

        coverage:
          - test/common/error

      - name: common/log
        lang: python

        coverage:
          - test/common/log

integration:
  - name: real
    db: true

    test:
      - name: all
        total: 1

performance:
  - name: performance

    test:
      - name: type
        total: 1
"""

# A define file holding a single unit test, which is all the test error cases need
DEFINE_ERROR = """
unit:
  - name: common

    test:
      - name: error
%s

integration: []
performance: []
"""

# A define file with a keyword that belongs to a test written at the module level instead
DEFINE_MODULE_ERROR = """
unit:
  - name: common
    total: 1

    test:
      - name: error

integration: []
performance: []
"""


####################################################################################################################################
def _def_parse(define):
    """Parse define yaml from a string by writing it where the parser looks for it."""

    with tempfile.TemporaryDirectory() as path:
        os.mkdir(os.path.join(path, "test"))

        with open(os.path.join(path, "test/define.yaml"), "w") as file:
            file.write(define)

        return test_def_parse(path)


####################################################################################################################################
def _def_parse_error(test_line_list):
    """Parse a define file holding a single test built from the lines given, indented to where a test declares its keys."""

    return _def_parse(DEFINE_ERROR % "\n".join("        " + line for line in test_line_list))


####################################################################################################################################
def test_define_module():
    """Every test becomes a module, in the order the file declares them."""

    module_list = _def_parse(DEFINE)

    assert_equal(
        [module.name for module in module_list],
        [
            "common/error",
            "common/stack-trace",
            "common/type-string",
            "test/common/error",
            "test/common/log",
            "real/all",
            "performance/type",
        ],
    )

    module = test_def_find(module_list, "common/stack-trace")

    assert_equal(module.type, TEST_TYPE_UNIT)
    assert_equal(module.total, 1)
    assert_true(module.bin_required)
    assert_true(module.container_required)
    assert_equal(module.flag, "-DDEBUG_TEST_TRACE")
    assert_equal(module.include_list, ["common/stackTrace"])

    # A feature is upper-cased and is available to the tests after the one that introduces it
    assert_equal(module.feature, "STACK-TRACE")
    assert_equal(module.feature_list, [])
    assert_equal(test_def_find(module_list, "common/type-string").feature_list, ["STACK-TRACE"])

    # PostgreSQL is required only by an integration module that declares it
    assert_false(module.pg_required)
    assert_true(test_def_find(module_list, "real/all").pg_required)

    # The type comes from the section the module was declared in
    assert_equal(test_def_find(module_list, "real/all").type, TEST_TYPE_INTEGRATION)
    assert_equal(test_def_find(module_list, "performance/type").type, TEST_TYPE_PERFORMANCE)


####################################################################################################################################
def test_define_lang_vm():
    """Language and vm may be declared for a whole module and overridden by a test."""

    module_list = _def_parse(DEFINE)

    # C is the default, and no vm means every vm
    assert_equal(test_def_find(module_list, "common/error").lang, TEST_LANG_C)
    assert_equal(test_def_find(module_list, "common/error").vm_list, [])

    # Declared for the whole module
    assert_equal(test_def_find(module_list, "test/common/error").lang, TEST_LANG_PYTHON)
    assert_equal(test_def_find(module_list, "test/common/error").vm_list, ["rh9"])

    # Declared by the test itself
    assert_equal(test_def_find(module_list, "test/common/log").lang, TEST_LANG_PYTHON)
    assert_equal(test_def_find(module_list, "common/type-string").vm_list, ["none", "u22"])


####################################################################################################################################
def test_define_coverage():
    """A coverage entry is a code module, optionally tagged as included in another module or as having no code."""

    module = test_def_find(_def_parse(DEFINE), "common/error")

    assert_equal([coverage.name for coverage in module.coverage_list], ["common/error", "common/errorInternal", "common/errorType"])

    # A bare name is coverable and compiled on its own
    assert_true(module.coverage_find("common/error").coverable)
    assert_false(module.coverage_find("common/error").included)

    # noCode has nothing to cover, e.g. a header of macros
    assert_false(module.coverage_find("common/errorInternal").coverable)
    assert_true(module.coverage_find("common/errorInternal").included)

    # included is covered but compiled as part of another module
    assert_true(module.coverage_find("common/errorType").coverable)
    assert_true(module.coverage_find("common/errorType").included)

    # A module that is not covered is reported by returning nothing
    assert_is_none(module.coverage_find("common/log"))


####################################################################################################################################
def test_define_depend():
    """Dependencies accumulate in declaration order and each language accumulates its own."""

    module_list = _def_parse(DEFINE)

    # The first test depends on nothing since it covers everything it declares
    assert_equal(test_def_find(module_list, "common/error").depend_list, [])

    # What the tests before it covered, plus what it declares. A module already accumulated is not added again, which is why a test
    # may safely declare a dependency that an earlier test has covered.
    assert_equal(test_def_find(module_list, "common/stack-trace").depend_list, ["common/error", "common/type/string"])

    # Minus what it covers or includes itself, since those are compiled into the test rather than depended on
    assert_equal(test_def_find(module_list, "common/type-string").depend_list, [])

    # A python test accumulates only python modules, since it cannot import a C one
    assert_equal(test_def_find(module_list, "test/common/error").depend_list, [])
    assert_equal(test_def_find(module_list, "test/common/log").depend_list, ["test/common/error"])

    # Accumulation carries on into the integration and performance tests
    assert_equal(test_def_find(module_list, "real/all").depend_list, ["common/error", "common/type/string"])


####################################################################################################################################
def test_define_harness():
    """Harnesses accumulate across tests and carry the modules they shim."""

    module_list = _def_parse(DEFINE)

    # A test declared before the harness does not have it
    assert_equal(test_def_find(module_list, "common/error").harness_list, [])

    # A bare name is a harness that includes nothing
    module = test_def_find(module_list, "common/stack-trace")

    assert_equal([harness.name for harness in module.harness_list], ["error"])
    assert_true(module.harness_list[0].integration)
    assert_equal(module.harness_list[0].include_list, [])
    assert_equal(module.shim_list, [])

    # A harness that shims modules includes them in the order they are listed
    module = test_def_find(module_list, "common/type-string")
    harness = module.harness_list[1]

    assert_equal([harness.name for harness in module.harness_list], ["error", "config"])
    assert_false(harness.integration)
    assert_equal(harness.include_list, ["common/config", "common/exec"])


####################################################################################################################################
def test_define_shim():
    """A shim names the functions the harness replaces, which may be defined in a module included by the shim."""

    module = test_def_find(_def_parse(DEFINE), "common/type-string")

    # A shimmed module with no functions is included in the harness but has nothing to shim
    assert_is_none(module.shim_find("common/config"))

    shim = module.shim_find("common/exec")

    assert_equal(shim.name, "common/exec")

    # The shim takes the integration flag of the harness that declared it
    assert_false(shim.integration)

    # A bare name is defined in the shim module, else the included module it is defined in is named
    assert_equal([function.name for function in shim.function_list], ["execOne", "execTwo"])
    assert_is_none(shim.function_list[0].inc)
    assert_equal(shim.function_list[1].inc, "common/execInc")


####################################################################################################################################
def test_define_error():
    """A definition the parser does not accept is an error that says what was wrong."""

    # A key that belongs to a test, written at the module level where it would be silently ignored
    with assert_raises(ToolError) as error:
        _def_parse(DEFINE_MODULE_ERROR)

    assert_equal(str(error.exception), "unexpected keyword 'total' in module 'common'")

    # Everything else is declared by a test
    for line_list, message in (
        (["bogus: true"], "unexpected keyword 'bogus' in test 'common/error'"),
        (["lang: rust"], "invalid lang 'rust' in test 'common/error'"),
        (["coverage:", "  - common/error: bogus"], "invalid coverage type bogus"),
        (["harness:", "  - name: config", "    bogus: true"], "invalid key 'bogus'"),
        (
            ["harness:", "  - name: config", "    shim:", "      - name: common/exec", "        bogus: true"],
            "invalid key 'bogus'",
        ),
        (["harness:", "  - name: config", "    shim:", "      - function:", "          - execOne"], "shim name is required"),
    ):
        with assert_raises(ToolError) as error:
            _def_parse_error(line_list)

        assert_equal(str(error.exception), message, line_list)

    # A test that is not in the file
    with assert_raises(ToolError) as error:
        test_def_find(_def_parse(DEFINE), "bogus/test")

    assert_equal(str(error.exception), "'bogus/test' is not a valid test")
