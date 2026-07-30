"""Virtual Machine Definitions.

The vms tests can run on, the PostgreSQL versions each one provides, and what the container build needs to know about them. A vm
name is what --vm accepts, e.g. u24 for Ubuntu 24.04.

The default list is checked when this module is imported: every supported PostgreSQL version must be tested on exactly one default
vm and at least one default vm must collect C coverage. Without the check a new version could be added and silently never tested."""

####################################################################################################################################
import platform

from common.error import TestError, check

# Os base a vm is built on, which determines its package manager and where PostgreSQL is installed
VM_OS_BASE_ALPINE = "alpine"
VM_OS_BASE_DEBIAN = "debian"
VM_OS_BASE_RHEL = "rhel"

# Architectures a container can be built for
VM_ARCH_AARCH64 = "aarch64"
VM_ARCH_I386 = "i386"
VM_ARCH_PPC64LE = "ppc64le"
VM_ARCH_S390X = "s390x"
VM_ARCH_X86_64 = "x86_64"

VM_ARCH_LIST = (VM_ARCH_AARCH64, VM_ARCH_I386, VM_ARCH_PPC64LE, VM_ARCH_S390X, VM_ARCH_X86_64)

# All default vms and no vm at all, i.e. run directly on the host
VM_ALL = "all"
VM_NONE = "none"

# Vms
VM_A321 = "a321"
VM_A324 = "a324"
VM_D12 = "d12"
VM_F44 = "f44"
VM_RH8 = "rh8"
VM_RH9 = "rh9"
VM_RH10 = "rh10"
VM_U22 = "u22"
VM_U24 = "u24"

# Default vms, i.e. the ones --vm=all selects
VM_LIST = (VM_D12, VM_RH8, VM_RH9, VM_RH10, VM_U22, VM_U24, VM_A321, VM_A324)

# PostgreSQL versions the project supports. Each one must be tested on a default vm, which vm_check() enforces.
PG_VERSION_LIST = ("9.6", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19")


####################################################################################################################################
class Vm:
    """A vm definition."""

    def __init__(self, os_base, db_list, image=None, db_test_list=None, pg_repo=True, coverage_c=False):
        self.os_base = os_base
        self.image = image  # Image the container is built from, None when the vm has no container
        self.pg_repo = pg_repo  # Install PostgreSQL from the PGDG repo rather than from the distribution?
        self.coverage_c = coverage_c  # Is C coverage collected on this vm?
        self.db_list = db_list  # PostgreSQL versions installed
        self.db_test_list = db_list if db_test_list is None else db_test_list  # Versions integration tests run against


####################################################################################################################################
# fmt: off
_VM = {
    # No container, i.e. run directly on the host
    VM_NONE: Vm(VM_OS_BASE_DEBIAN, ["10"], coverage_c=True),

    # Alpine 3.21
    VM_A321: Vm(VM_OS_BASE_ALPINE, ["15", "16", "17"], image="alpine:3.21", db_test_list=["16"], pg_repo=False),

    # Alpine 3.24
    VM_A324: Vm(VM_OS_BASE_ALPINE, ["16", "17", "18"], image="alpine:3.24", db_test_list=["17"], pg_repo=False),

    # Debian 12
    VM_D12: Vm(VM_OS_BASE_DEBIAN, ["15"], image="debian:12", pg_repo=False),

    # RHEL 8
    VM_RH8: Vm(VM_OS_BASE_RHEL, ["10"], image="rockylinux/rockylinux:8", pg_repo=False),

    # RHEL 9
    VM_RH9: Vm(VM_OS_BASE_RHEL, ["14", "15", "16", "17", "18"], image="rockylinux/rockylinux:9", db_test_list=["14"]),

    # RHEL 10
    VM_RH10: Vm(VM_OS_BASE_RHEL, ["14", "15", "16", "17", "18"], image="rockylinux/rockylinux:10", db_test_list=["18"]),

    # Fedora 44
    VM_F44: Vm(
        VM_OS_BASE_RHEL, ["14", "15", "16", "17", "18"], image="fedora:44", db_test_list=["15"], coverage_c=True),

    # Ubuntu 22.04
    VM_U22: Vm(
        VM_OS_BASE_DEBIAN, ["9.6", "10", "11", "12", "13", "14"], image="ubuntu:22.04", db_test_list=["9.6", "11"],
        coverage_c=True),

    # Ubuntu 24.04
    VM_U24: Vm(
        VM_OS_BASE_DEBIAN, ["9.6", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19"], image="ubuntu:24.04",
        db_test_list=["12", "13", "19"], coverage_c=True),
}
# fmt: on


####################################################################################################################################
def vm_valid(vm):
    """Is there a definition for this vm?"""

    return vm in _VM


####################################################################################################################################
def vm_get(vm):
    """The definition for a vm, which also validates that the vm exists."""

    if not vm_valid(vm):
        raise TestError("no definition for vm '%s'" % vm)

    return _VM[vm]


####################################################################################################################################
def vm_check(vm_map):
    """Check that C coverage and every supported PostgreSQL version are tested on a default vm.

    Called below on the definitions in this module, so a version that no vm tests is an error rather than a silent gap."""

    check(any(vm_map[vm].coverage_c for vm in VM_LIST), "C coverage is not configured to run on a default vm")

    for version in PG_VERSION_LIST:
        vm_found = None

        for vm in VM_LIST:
            if version in vm_map[vm].db_test_list:
                check(vm_found is None, "PostgreSQL %s is already configured to run on default vm %s" % (version, vm_found))

                vm_found = vm

        check(vm_found is not None, "PostgreSQL %s is not configured to run on a default vm" % version)


####################################################################################################################################
def host_arch():
    """Architecture of the host, using the names the project uses.

    Mac reports arm64 where Linux reports aarch64 and 32-bit x86 reports i686 where the project uses i386."""

    arch = platform.machine()

    return {"arm64": VM_ARCH_AARCH64, "i686": VM_ARCH_I386}.get(arch, arch)


####################################################################################################################################
vm_check(_VM)
