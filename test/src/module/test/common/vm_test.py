"""Test Virtual Machine Definitions.

The definitions in the module are checked when it is imported, so a broken definition would fail before any test here runs. The
checks are exercised again with definitions built here, since the real ones can only ever be valid."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.vm import *


####################################################################################################################################
def _vm_map(db_test_list=None, coverage_c=True):
    """Build a vm map that covers every default vm, which is what the check looks at.

    Every supported version is tested on the first vm unless the caller says otherwise."""

    result = {vm: Vm(VM_OS_BASE_DEBIAN, []) for vm in VM_LIST}
    result[VM_LIST[0]] = Vm(
        VM_OS_BASE_DEBIAN, list(PG_VERSION_LIST) if db_test_list is None else db_test_list, coverage_c=coverage_c
    )

    return result


####################################################################################################################################
def test_vm_get():
    """A vm definition is found by name and a name with no definition is an error."""

    assert_true(vm_valid(VM_U24))
    assert_false(vm_valid("bogus"))

    vm = vm_get(VM_U24)

    assert_equal(vm.os_base, VM_OS_BASE_DEBIAN)
    assert_equal(vm.image, "ubuntu:24.04")
    assert_true(vm.pg_repo)
    assert_true(vm.coverage_c)

    # A vm that does not install from the PostgreSQL repo and tests every version it installs
    vm = vm_get(VM_D12)

    assert_false(vm.pg_repo)
    assert_false(vm.coverage_c)
    assert_equal(vm.db_list, vm.db_test_list)

    # There is no container for none, since it runs on the host
    assert_is_none(vm_get(VM_NONE).image)

    with assert_raises(ToolError) as error:
        vm_get("bogus")

    assert_equal(str(error.exception), "no definition for vm 'bogus'")


####################################################################################################################################
def test_vm_check():
    """Definitions that leave a version or C coverage untested are an error."""

    assert_is_none(vm_check(_vm_map()))

    # C coverage must be collected somewhere, else a change could stop being checked without anything reporting it
    with assert_raises(ToolError) as error:
        vm_check(_vm_map(coverage_c=False))

    assert_equal(str(error.exception), "C coverage is not configured to run on a default vm")

    # A version tested on more than one vm is an error since the extra run is just slower with nothing added
    vm_map = _vm_map()
    vm_map[VM_LIST[1]] = Vm(VM_OS_BASE_DEBIAN, [PG_VERSION_LIST[0]])

    with assert_raises(ToolError) as error:
        vm_check(vm_map)

    assert_equal(
        str(error.exception), "PostgreSQL %s is already configured to run on default vm %s" % (PG_VERSION_LIST[0], VM_LIST[0])
    )

    # A version tested on no vm is an error, which is what catches a version that was added but never wired up
    with assert_raises(ToolError) as error:
        vm_check(_vm_map(db_test_list=list(PG_VERSION_LIST[1:])))

    assert_equal(str(error.exception), "PostgreSQL %s is not configured to run on a default vm" % PG_VERSION_LIST[0])


####################################################################################################################################
def test_vm_arch():
    """The host architecture is reported with the name the project uses."""

    assert_true(host_arch() in VM_ARCH_LIST)
