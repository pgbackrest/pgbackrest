"""Test List.

Selects the tests to run from the definitions and the options. A test module produces one run per vm, and an integration module
produces one run per test and PostgreSQL version as well, since each of those needs its own container."""

####################################################################################################################################
from command.test.define import TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE
from common.error import check
from common.vm import *


####################################################################################################################################
class TestRun:
    """A test module to run on one vm, for one PostgreSQL version and one test in the module where those apply."""

    def __init__(self, module, vm, pg_version, test_list):
        self.module = module
        self.vm = vm
        self.pg_version = pg_version  # PostgreSQL version to test against, None when the test does not need one
        self.test_list = test_list  # Tests in the module to run, None for all of them

    ################################################################################################################################
    @property
    def integration(self):
        """Is this an integration test?

        An integration test runs the pgbackrest binary rather than a unit test, so there is nothing to build for it and it starts
        its own containers."""

        return self.module.type == TEST_TYPE_INTEGRATION

    ################################################################################################################################
    @property
    def performance(self):
        """Is this a performance test?

        A performance test is built and run like a unit test but is timed, so debugging and coverage are left out of it."""

        return self.module.type == TEST_TYPE_PERFORMANCE

    ################################################################################################################################
    @property
    def coverage_name(self):
        """Name of the raw coverage file for this test.

        A test name contains a path separator so flatten it, since the coverage command reads the raw files from one directory."""

        return self.module.name.replace("/", "-")


####################################################################################################################################
def _selected(module_name, name):
    """Was this test module selected by a name given on the command line?

    A name is either a test module or the path of a group of them, e.g. common/type/string is the string test while common/type is
    every test under it. Case is ignored, as it is in the module paths the names come from."""

    module_name = module_name.lower()
    name = name.lower()

    return module_name == name or module_name.startswith(name + "/")


####################################################################################################################################
def _check(module_list, config):
    """Check the selection against the definitions.

    A name that matches nothing is a typo, which is worth reporting rather than running whatever else was selected as if nothing
    were wrong."""

    for name in config.module:
        check(any(_selected(module.name, name) for module in module_list), "'%s' does not match a test module" % name)

    # A test is numbered within a single module, so the path of a group of them has nothing to apply it to
    check(
        not config.test
        or (len(config.module) == 1 and any(module.name.lower() == config.module[0].lower() for module in module_list)),
        "--test requires a single --module naming a test module",
    )

    # A version the project does not support is a typo, which is worth reporting for the same reason a module name is
    check(
        config.pg_version in (PG_VERSION_ALL, PG_VERSION_MINIMAL) + PG_VERSION_LIST,
        "'%s' does not match a supported PostgreSQL version" % config.pg_version,
    )


####################################################################################################################################
def test_list_get(module_list, config):
    """Build the list of tests to run."""

    _check(module_list, config)

    result = []

    for vm_name in VM_LIST if config.vm == VM_ALL else [config.vm]:
        vm = vm_get(vm_name)

        for module in module_list:
            # An empty selection runs everything, which is what a run with no --module does
            if config.module and not any(_selected(module.name, name) for name in config.module):
                continue

            # Skip this test if it does not run on this vm
            if module.vm_list and vm_name not in module.vm_list:
                continue

            integration = module.type == TEST_TYPE_INTEGRATION

            # Skip this test if only C tests were requested, which leaves out the integration tests since they run the pgbackrest
            # binary rather than a unit test
            if config.c_only and integration:
                continue

            # Skip this test if it needs a container and there is none. Integration tests always need one.
            if config.vm == VM_NONE and (integration or module.container_required):
                continue

            # Skip this test if it is a performance test and those were not requested
            if not config.performance and module.type == TEST_TYPE_PERFORMANCE:
                continue

            # Skip this test if it does not need a container and only tests that need one were requested
            if config.container_only and not integration and not module.container_required:
                continue

            # Skip this test if only tests that provide coverage were requested
            if config.coverage_only and not module.coverage_list:
                continue

            # PostgreSQL versions to test against, most recent first. Only an integration module declares that it needs them.
            #
            # A run that does not name a version tests the versions the vm declares, which is the minimal set that covers every
            # supported version across the default vms. A named version runs on every vm that installs it and --pg-version=all
            # runs every version a vm installs, since what a vm declares only spreads the versions over the default vms rather
            # than limiting what it is able to test.
            pg_version_list = [None]

            if module.pg_required:
                if config.pg_version == PG_VERSION_MINIMAL:
                    pg_version_list = vm.db_test_list
                elif config.pg_version == PG_VERSION_ALL:
                    pg_version_list = vm.db_list
                else:
                    pg_version_list = [version for version in vm.db_list if version == config.pg_version]

                pg_version_list = list(reversed(pg_version_list))

            for pg_version in pg_version_list:
                # An integration module runs each of its tests in its own container, so each one is a separate run here. Every
                # other module runs all of its tests at once and passes on whatever --test selected.
                if not integration:
                    result.append(TestRun(module, vm_name, pg_version, config.test if config.test else None))

                    continue

                for test in range(1, module.total + 1):
                    if config.test and test not in config.test:
                        continue

                    result.append(TestRun(module, vm_name, pg_version, [test]))

    return result
