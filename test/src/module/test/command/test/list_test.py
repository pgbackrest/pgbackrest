"""Test Test List.

The modules are built here rather than read from define.yaml so the selection can be checked against definitions that do not exist
yet, e.g. a test that only runs on one vm."""

####################################################################################################################################
from harness.test import *

from command.test.define import TEST_LANG_PYTHON, TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE, TEST_TYPE_UNIT, TestDefModule
from command.test.list import *
from common.vm import *


####################################################################################################################################
class _Config:
    """The options the selection reads, with the defaults a run with no options would have."""

    def __init__(self, **option):
        self.vm = VM_NONE
        self.module = []
        self.test = []
        self.run = []
        self.pg_version = "minimal"
        self.c_only = False
        self.container_only = False
        self.coverage_only = False
        self.performance = True

        self.__dict__.update(option)


####################################################################################################################################
def _module(name, type=TEST_TYPE_UNIT, **field):
    """Build a test module definition."""

    result = TestDefModule(name, type)

    for key, value in field.items():
        setattr(result, key, value)

    return result


####################################################################################################################################
def _name_list(test_list):
    """The name of each selected test, which is what most of the selection rules decide."""

    return [run.module.name for run in test_list]


# Modules that cover every kind of test the selection has a rule for
MODULE_LIST = [
    _module("common/error", coverage_list=["common/error"]),
    _module("common/exec"),
    _module("common/socket", container_required=True),
    _module("doc/build", vm_list=[VM_U24]),
    _module("test/common/log", lang=TEST_LANG_PYTHON, coverage_list=["test/common/log"]),
    _module("integration/all", type=TEST_TYPE_INTEGRATION, total=3, pg_required=True),
    _module("performance/type", type=TEST_TYPE_PERFORMANCE, total=2),
]


####################################################################################################################################
def test_list_all():
    """Everything that can run on a vm is selected when nothing is asked for."""

    # Integration tests need a container so none of them run without one, and neither does a test that requires one
    assert_equal(
        _name_list(test_list_get(MODULE_LIST, _Config())),
        ["common/error", "common/exec", "test/common/log", "performance/type"],
    )

    # A vm with a container runs everything, and the integration test runs once per sub-test and PostgreSQL version
    test_list = test_list_get(MODULE_LIST, _Config(vm=VM_U24))

    assert_equal(
        _name_list(test_list),
        ["common/error", "common/exec", "common/socket", "doc/build", "test/common/log"]
        + ["integration/all"] * 3 * len(vm_get(VM_U24).db_test_list)
        + ["performance/type"],
    )

    # A test that only runs on one vm is left out of the others
    assert_true("doc/build" not in _name_list(test_list_get(MODULE_LIST, _Config(vm=VM_D12))))

    # Every default vm runs its own copy of the list
    assert_equal(len(set(run.vm for run in test_list_get(MODULE_LIST, _Config(vm=VM_ALL)))), len(VM_LIST))


####################################################################################################################################
def test_list_select():
    """A module and a test in it are selected by name, ignoring case."""

    assert_equal(_name_list(test_list_get(MODULE_LIST, _Config(module=["common"]))), ["common/error", "common/exec"])
    assert_equal(_name_list(test_list_get(MODULE_LIST, _Config(module=["COMMON"], test=["Error"]))), ["common/error"])

    # A python test is named for the group it is in and the path of the module it covers
    assert_equal(_name_list(test_list_get(MODULE_LIST, _Config(module=["test"], test=["common/log"]))), ["test/common/log"])

    # A name that matches nothing selects nothing rather than being an error, since the test command reports that
    assert_equal(_name_list(test_list_get(MODULE_LIST, _Config(module=["bogus"]))), [])


####################################################################################################################################
def test_list_filter():
    """The filters leave out the kinds of test they name."""

    # Only integration tests are not C, so this is the only thing --c-only leaves out
    assert_true("integration/all" not in _name_list(test_list_get(MODULE_LIST, _Config(vm=VM_U24, c_only=True))))

    # Only tests that must have a container, which includes every integration test
    assert_equal(
        _name_list(test_list_get(MODULE_LIST, _Config(vm=VM_U24, container_only=True))),
        ["common/socket"] + ["integration/all"] * 3 * len(vm_get(VM_U24).db_test_list),
    )

    # Only tests that provide coverage, which is what the documentation summary is built from
    assert_equal(_name_list(test_list_get(MODULE_LIST, _Config(coverage_only=True))), ["common/error", "test/common/log"])

    # Performance tests are timed so they are left out of a run that is only checking behavior
    assert_true("performance/type" not in _name_list(test_list_get(MODULE_LIST, _Config(performance=False))))


####################################################################################################################################
def test_list_pg_version():
    """An integration test runs against the PostgreSQL versions the vm tests, most recent first."""

    version_list = vm_get(VM_U24).db_test_list

    assert_equal(
        [run.pg_version for run in test_list_get(MODULE_LIST, _Config(vm=VM_U24, module=["integration"]))],
        [version for version in reversed(version_list) for _ in range(3)],
    )

    # A single version can be selected, which is how a version specific problem is chased down
    assert_equal(
        [run.pg_version for run in test_list_get(MODULE_LIST, _Config(vm=VM_U24, module=["integration"], pg_version="12"))],
        ["12"] * 3,
    )

    # A version that the vm does not test selects nothing
    assert_equal(
        test_list_get(MODULE_LIST, _Config(vm=VM_U24, module=["integration"], pg_version="8.0")),
        [],
    )


####################################################################################################################################
def test_list_run():
    """Each sub-test of an integration test is a run of its own, since each one needs its own containers."""

    test_list = test_list_get(MODULE_LIST, _Config(vm=VM_U24, module=["integration"], pg_version="12"))

    assert_equal([run.run_list for run in test_list], [[1], [2], [3]])
    assert_equal(test_list[0].coverage_name, "integration-all")
    assert_true(test_list[0].integration)
    assert_false(test_list[0].performance)

    # A run can be selected, which is the only way to get one container out of an integration test
    assert_equal(
        [run.run_list for run in test_list_get(MODULE_LIST, _Config(vm=VM_U24, module=["integration"], pg_version="12", run=[2]))],
        [[2]],
    )

    # Every other kind of test runs all of its sub-tests at once, so the selection is passed on for the test itself to apply
    test_list = test_list_get(MODULE_LIST, _Config(module=["performance"]))

    assert_equal([run.run_list for run in test_list], [None])
    assert_true(test_list[0].performance)
    assert_false(test_list[0].integration)

    assert_equal([run.run_list for run in test_list_get(MODULE_LIST, _Config(module=["performance"], run=[2]))], [[2]])

    # A python test name contains a path separator, which is flattened for the coverage file since they share a path
    assert_equal(test_list_get(MODULE_LIST, _Config(module=["test"]))[0].coverage_name, "test-common-log")
