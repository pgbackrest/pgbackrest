"""Virtual Machine Definitions.

The vms tests can run on, the PostgreSQL versions each one provides, and what the container build needs to know about them. A vm
name is what --vm accepts, e.g. u24 for Ubuntu 24.04.

The definitions are declared in test/container.yaml and are read when this module is imported, so a definition that does not parse
fails before any tool that reads it runs. The declaration is checked as well: every supported PostgreSQL version must be tested on
exactly one default vm and at least one default vm must collect C coverage. Without the check a new version could be added and
silently never tested.

The versions the project supports are not declared here either. They come from build/postgres.yaml, which is what the version
interfaces are generated from, so a version is supported and tested because one declaration says so."""

####################################################################################################################################
import os
import platform

from common.error import ToolError, check
from common.storage import file_read
from common.yaml import yaml_bool, yaml_load, yaml_map_dict
from postgres.parse import bld_pg_version_list

# Os base a vm is built on, which determines its package manager and where PostgreSQL is installed
VM_OS_BASE_ALPINE = "alpine"
VM_OS_BASE_DEBIAN = "debian"
VM_OS_BASE_RHEL = "rhel"

VM_OS_BASE_LIST = (VM_OS_BASE_ALPINE, VM_OS_BASE_DEBIAN, VM_OS_BASE_RHEL)

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

# Where the python coverage package comes from, since not every vm packages a version new enough to report branch detail
_VM_COVERAGE_PYTHON_LIST = ("package", "pip")

# Declaration the definitions are read from, named as the tools report it rather than as it is opened
VM_PATH_CONTAINER = "test/container.yaml"

# Repository the declarations are read from. This module is part of the repository, so its own path locates it, and a tool imports
# it before it has a repository path of its own to give.
_PATH_REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# Supported PostgreSQL versions, oldest first
_PG_LIST = bld_pg_version_list(_PATH_REPO)

# PostgreSQL versions the project supports. Each one must be tested on a default vm, which vm_check() enforces.
PG_VERSION_LIST = tuple(pg.version for pg in _PG_LIST)

# Versions that have not been released yet, which are only in the beta repository
_PG_BETA_LIST = tuple(pg.version for pg in _PG_LIST if not pg.release)

# Keys a vm with no container may declare, since every other key says how a container is built
_VM_KEY_HOST_LIST = ("coverage-c", "default")

# Keys a vm may declare
_VM_KEY_LIST = (
    "coverage-c",
    "coverage-python",
    "db",
    "db-test",
    "default",
    "dnf-module",
    "epel",
    "image",
    "os-base",
    "pg-repo",
    "pg-repo-key",
    "pg-repo-release",
    "powertools",
    "python",
    "ssh-rsa",
    "valgrind",
)


####################################################################################################################################
class Vm:
    """A vm definition."""

    def __init__(self, name):
        self.name = name
        self.default = False  # Is this a default vm, i.e. one of the vms --vm=all selects?
        self.os_base = None  # Os the container is built on, which determines its package manager and where PostgreSQL is installed
        self.image = None  # Image the container is built from, None when the vm has no container
        self.db_list = []  # PostgreSQL versions installed
        self.db_test_list = []  # Versions integration tests run against, which is every version installed unless declared
        self.pg_beta = None  # Unreleased version installed, which is only in the beta repository
        self.pg_repo = True  # Install PostgreSQL from the PGDG repo rather than from the distribution?
        self.pg_repo_key = True  # Import the PGDG signing key, which not every rpm backend accepts?
        self.pg_repo_release = None  # Release the PGDG repo package is built for, e.g. EL-9
        self.dnf_module = False  # Disable the distribution's postgresql dnf module so the PGDG packages are used?
        self.coverage_c = False  # Is C coverage collected on this vm?
        self.coverage_python = None  # Where the python coverage package comes from, None when python coverage is not collected
        self.valgrind = True  # Install valgrind?
        self.epel = False  # Install EPEL and enable CRB, which is where the packages rhel does not ship itself are?
        self.powertools = False  # Enable powertools, which is what CRB was called before EL-9?
        self.python = None  # Python to install and make the default, when the platform python is not the one to use
        self.ssh_rsa = False  # Add back the ssh-rsa algorithms, which SFTP needs and a newer sshd rejects by default?


####################################################################################################################################
def _vm_parse(name, definition):
    """Parse a single vm definition."""

    result = Vm(name)
    key_map = yaml_map_dict(definition, "vm '%s'" % name)

    for key in key_map:
        check(key in _VM_KEY_LIST, "unknown key '%s' in vm '%s'" % (key, name))

    def flag(key, default="false"):
        """A boolean key, which defaults to what a vm that does not declare it gets."""

        return yaml_bool(key_map.get(key, default), "vm '%s' key '%s'" % (name, key))

    def version_list(key, default):
        """A list of PostgreSQL versions, which must be written as a list even when there is only one of them."""

        value = key_map.get(key, default)

        check(isinstance(value, list), "vm '%s' key '%s' must be a list" % (name, key))

        return list(value)

    result.default = flag("default")
    result.os_base = key_map.get("os-base")
    result.image = key_map.get("image")
    result.db_list = version_list("db", [])
    result.db_test_list = version_list("db-test", result.db_list)
    result.pg_repo = flag("pg-repo", "true")
    result.pg_repo_key = flag("pg-repo-key", "true")
    result.pg_repo_release = key_map.get("pg-repo-release")
    result.dnf_module = flag("dnf-module")
    result.coverage_c = flag("coverage-c")
    result.coverage_python = key_map.get("coverage-python")
    result.valgrind = flag("valgrind", "true")
    result.epel = flag("epel")
    result.powertools = flag("powertools")
    result.python = key_map.get("python")
    result.ssh_rsa = flag("ssh-rsa")

    # A vm with no image has no container to build, so it declares nothing about how one would be built. That includes the os it
    # runs on, which is whatever the host is rather than anything this can know.
    if result.image is None:
        for key in key_map:
            check(key in _VM_KEY_HOST_LIST, "vm '%s' has no container so it may not declare '%s'" % (name, key))
    else:
        check(
            result.os_base in VM_OS_BASE_LIST,
            "vm '%s' must declare one of these os bases: %s" % (name, ", ".join(VM_OS_BASE_LIST)),
        )

    check(
        result.coverage_python in (None,) + _VM_COVERAGE_PYTHON_LIST,
        "invalid python coverage '%s' in vm '%s'" % (result.coverage_python, name),
    )

    for db in result.db_list:
        check(db in PG_VERSION_LIST, "PostgreSQL %s in vm '%s' is not a supported version" % (db, name))

    for db in result.db_test_list:
        check(db in result.db_list, "PostgreSQL %s tested on vm '%s' is not installed there" % (db, name))

    # An unreleased version comes from the beta repository, which the container build needs to add
    result.pg_beta = next((db for db in result.db_list if db in _PG_BETA_LIST), None)

    return result


####################################################################################################################################
def vm_parse(content):
    """Parse the vm definitions, keyed by vm name."""

    section = yaml_map_dict(yaml_load(content, VM_PATH_CONTAINER), VM_PATH_CONTAINER)

    check("vm" in section, "the 'vm' section is required in %s" % VM_PATH_CONTAINER)

    return {
        name: _vm_parse(name, definition)
        for name, definition in yaml_map_dict(section["vm"], "the 'vm' section in %s" % VM_PATH_CONTAINER).items()
    }


####################################################################################################################################
def vm_valid(vm):
    """Is there a definition for this vm?"""

    return vm in _VM


####################################################################################################################################
def vm_get(vm):
    """The definition for a vm, which also validates that the vm exists."""

    if not vm_valid(vm):
        raise ToolError("no definition for vm '%s'" % vm)

    return _VM[vm]


####################################################################################################################################
def vm_default_list(vm_map):
    """The default vms in a definition map, i.e. the ones --vm=all selects."""

    return tuple(name for name, vm in vm_map.items() if vm.default)


####################################################################################################################################
def vm_check(vm_map):
    """Check that C coverage and every supported PostgreSQL version are tested on a default vm.

    Called below on the definitions that were read, so a version that no vm tests is an error rather than a silent gap."""

    vm_list = vm_default_list(vm_map)

    check(any(vm_map[vm].coverage_c for vm in vm_list), "C coverage is not configured to run on a default vm")

    for version in PG_VERSION_LIST:
        vm_found = None

        for vm in vm_list:
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
# Definitions, read and checked on import so a tool that reads them never has to
_VM = vm_parse(file_read(os.path.join(_PATH_REPO, VM_PATH_CONTAINER)))

vm_check(_VM)

# Default vms, i.e. the ones --vm=all selects
VM_LIST = vm_default_list(_VM)
