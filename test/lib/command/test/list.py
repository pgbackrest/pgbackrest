"""Test List.

Selects the tests to run from the definitions and the options. A test module produces one run per vm, and an integration module
produces one run per sub-test and PostgreSQL version as well, since each of those needs its own container."""

####################################################################################################################################
from command.test.define import TEST_TYPE_INTEGRATION, TEST_TYPE_PERFORMANCE
from common.vm import *


####################################################################################################################################
class TestRun:
    """A test module to run on one vm, for one PostgreSQL version and one sub-test where those apply."""

    def __init__(self, module, vm, pg_version, run_list):
        self.module = module
        self.vm = vm
        self.pg_version = pg_version  # PostgreSQL version to test against, None when the test does not need one
        self.run_list = run_list  # Sub-tests to run, None for all of them

        # The module name is the group and the test in it, e.g. the common group and the error test in it
        self.group, _, self.test = module.name.partition("/")

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
def _selected(name_list, name):
    """Was this module or test selected on the command line?

    An empty list selects everything, which is what running with no --module or --test does."""

    return not name_list or name.lower() in [selected.lower() for selected in name_list]


####################################################################################################################################
def test_list_get(module_list, config):
    """Build the list of tests to run."""

    result = []

    for vm_name in VM_LIST if config.vm == VM_ALL else [config.vm]:
        vm = vm_get(vm_name)

        for module in module_list:
            # The module name is the group and the test in it, e.g. the common group and the error test in it
            group, _, test = module.name.partition("/")

            if not _selected(config.module, group) or not _selected(config.test, test):
                continue

            # Skip this test if it does not run on this vm
            if module.vm_list and vm_name not in module.vm_list:
                continue

            integration = module.type == TEST_TYPE_INTEGRATION

            # Skip this test if only C tests were requested. An integration test runs the pgbackrest binary rather than a unit
            # test, so it is the only kind that is not C.
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
            pg_version_list = list(reversed(vm.db_test_list)) if module.pg_required else [None]

            for pg_version in pg_version_list:
                if pg_version is not None and config.pg_version not in ("all", "minimal", pg_version):
                    continue

                # An integration test runs each sub-test in its own container, so each one is a separate run here. Every other
                # test runs all of its sub-tests at once and passes on whatever --run selected.
                if not integration:
                    result.append(TestRun(module, vm_name, pg_version, config.run if config.run else None))

                    continue

                for run in range(1, module.total + 1):
                    if config.run and run not in config.run:
                        continue

                    result.append(TestRun(module, vm_name, pg_version, [run]))

    return result
