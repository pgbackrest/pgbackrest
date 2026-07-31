"""Test Virtual Machine Definitions.

The definitions in test/container.yaml are read and checked when the module is imported, so a broken declaration would fail before
any test here runs. The parsing and the checks are exercised again with declarations written here, since the real one can only ever
be valid."""

####################################################################################################################################
from harness.test import *

from common.error import *
from common.vm import *

# A declaration with one vm in it, which is enough for the keys that are read one vm at a time
VM_ONE = """
vm:
  u24:
    default: true
    os-base: debian
    image: ubuntu:24.04
    db: [17, 18]
    db-test: [18]
    coverage-c: true
"""


####################################################################################################################################
def _vm_map(db_test_list=None, coverage_c=True):
    """Build a vm map that covers every default vm, which is what the check looks at.

    Every supported version is tested on the first vm unless the caller says otherwise."""

    result = {}

    for name in VM_LIST:
        vm = Vm(name)
        vm.default = True
        result[name] = vm

    result[VM_LIST[0]].db_test_list = list(PG_VERSION_LIST) if db_test_list is None else db_test_list
    result[VM_LIST[0]].coverage_c = coverage_c

    return result


####################################################################################################################################
def test_vm_get():
    """A vm definition is found by name and a name with no definition is an error."""

    assert_true(vm_valid("u24"))
    assert_false(vm_valid("bogus"))

    vm = vm_get("u24")

    assert_equal(vm.name, "u24")
    assert_equal(vm.os_base, VM_OS_BASE_DEBIAN)
    assert_equal(vm.image, "ubuntu:24.04")
    assert_true(vm.pg_repo)
    assert_true(vm.coverage_c)

    # A vm that does not install from the PostgreSQL repo and tests every version it installs
    vm = vm_get("d12")

    assert_false(vm.pg_repo)
    assert_false(vm.coverage_c)
    assert_equal(vm.db_list, vm.db_test_list)

    # There is no container for none, since it runs on the host, and it is not a vm --vm=all selects. What would be installed in a
    # container is not declared for it, and neither is an os base since the os is whatever the host is.
    vm = vm_get(VM_NONE)

    assert_is_none(vm.image)
    assert_is_none(vm.os_base)
    assert_equal(vm.db_list, [])
    assert_true(vm.coverage_c)
    assert_true(VM_NONE not in VM_LIST)

    with assert_raises(ToolError) as error:
        vm_get("bogus")

    assert_equal(str(error.exception), "no definition for vm 'bogus'")


####################################################################################################################################
def _parse(key):
    """Parse a vm that has a container, since that is what all but a couple of the keys are for."""

    return vm_parse("vm:\n  u24:\n    image: ubuntu:24.04\n%s" % key)["u24"]


####################################################################################################################################
def test_vm_parse():
    """A definition is read as it is declared and what it does not declare gets a default."""

    vm = vm_parse(VM_ONE)["u24"]

    assert_true(vm.default)
    assert_equal(vm.os_base, VM_OS_BASE_DEBIAN)
    assert_equal(vm.image, "ubuntu:24.04")
    assert_equal(vm.db_list, ["17", "18"])
    assert_equal(vm.db_test_list, ["18"])
    assert_true(vm.coverage_c)

    # Everything that was not declared, which is what a vm that needs nothing installed or configured gets
    assert_true(vm.pg_repo)
    assert_true(vm.pg_repo_key)
    assert_is_none(vm.pg_repo_release)
    assert_false(vm.dnf_module)
    assert_is_none(vm.coverage_python)
    assert_true(vm.valgrind)
    assert_false(vm.epel)
    assert_false(vm.powertools)
    assert_is_none(vm.python)
    assert_false(vm.ssh_rsa)

    # Integration tests run against every version installed when the vm does not say which
    vm = _parse("    os-base: debian\n    db: [17, 18]\n")

    assert_equal(vm.db_test_list, ["17", "18"])
    assert_false(vm.default)

    # An unreleased version is only in the beta repository, so the container build is told which installed version needs it
    assert_is_none(vm.pg_beta)
    assert_true(vm_get("u24").pg_beta in vm_get("u24").db_list)

    # A vm with no image has no container, so it declares only whether it is a default and whether it collects C coverage
    vm = vm_parse("vm:\n  none:\n    coverage-c: true\n")["none"]

    assert_is_none(vm.image)
    assert_is_none(vm.os_base)
    assert_equal(vm.db_list, [])
    assert_equal(vm.db_test_list, [])
    assert_true(vm.coverage_c)


####################################################################################################################################
def test_vm_parse_error():
    """A declaration that is not one is reported, since a key that is ignored would silently change what is installed."""

    with assert_raises(ToolError) as error:
        vm_parse("bogus:\n")

    assert_equal(str(error.exception), "the 'vm' section is required in test/container.yaml")

    with assert_raises(ToolError) as error:
        vm_parse("vm:\n  - u24\n")

    assert_equal(str(error.exception), "the 'vm' section in test/container.yaml must be a map")

    with assert_raises(ToolError) as error:
        vm_parse("vm:\n  u24: bogus\n")

    assert_equal(str(error.exception), "vm 'u24' must be a map")

    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    bogus: true\n")

    assert_equal(str(error.exception), "unknown key 'bogus' in vm 'u24'")

    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    default: yes\n")

    assert_equal(str(error.exception), "invalid boolean 'yes' for vm 'u24' key 'default'")

    # A container has to be built on something, so an os base that is not one of them, or none at all, is reported the same way
    with assert_raises(ToolError) as error:
        _parse("    os-base: bogus\n")

    assert_equal(str(error.exception), "vm 'u24' must declare one of these os bases: alpine, debian, rhel")

    # A vm with no container is not built, so a key that says how one would be built could never be used
    with assert_raises(ToolError) as error:
        vm_parse("vm:\n  none:\n    os-base: debian\n")

    assert_equal(str(error.exception), "vm 'none' has no container so it may not declare 'os-base'")

    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    coverage-python: bogus\n")

    assert_equal(str(error.exception), "invalid python coverage 'bogus' in vm 'u24'")

    # A version written on its own is a string, which would be read as a list of the characters in it
    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    db: 17\n")

    assert_equal(str(error.exception), "vm 'u24' key 'db' must be a list")

    # A version no interface is generated for could never be tested, so it is a typo rather than a version
    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    db: [17, 99]\n")

    assert_equal(str(error.exception), "PostgreSQL 99 in vm 'u24' is not a supported version")

    # A version that is tested but not installed would be selected for an integration test that could never run
    with assert_raises(ToolError) as error:
        _parse("    os-base: debian\n    db: [17]\n    db-test: [18]\n")

    assert_equal(str(error.exception), "PostgreSQL 18 tested on vm 'u24' is not installed there")


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
    vm_map[VM_LIST[1]].db_test_list = [PG_VERSION_LIST[0]]

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
