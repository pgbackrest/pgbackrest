"""Python Test Runner.

Runs a python test module with only the library modules it declared in define.yaml importable. A C unit test compiles in just the
modules it declares, so reaching for anything else fails the build and every cross-dependency ends up documented. Python has no link
step and would happily import anything under test/lib, so an import hook reproduces that.

This runs as a script rather than inside the harness because the harness has already imported most of the library, and an import
that is already in sys.modules never reaches a hook."""

####################################################################################################################################
import argparse
import importlib.util
import os
import sys


####################################################################################################################################
class ImportGuard:
    """Refuse to import a library module the test did not declare."""

    def __init__(self, path_lib, allow_list):
        self.path_lib = path_lib

        # The test harness is support for the tests rather than part of the harness under test, so it is always available
        self.allow_list = set(allow_list) | {"harness.test"}

        # Top level packages of the library, i.e. the imports that are guarded. Anything else, e.g. the standard library, is left
        # to the finders that follow this one.
        self.root_list = {name for name in os.listdir(path_lib) if os.path.isdir(os.path.join(path_lib, name))}

    ################################################################################################################################
    def find_spec(self, fullname, path=None, target=None):
        """Refuse a library module the test did not declare, leaving everything else to the finders that follow."""

        if fullname.split(".")[0] not in self.root_list or fullname in self.allow_list:
            return None

        # A package holds no code of its own, and a module is reached through the packages above it, so only modules are declared
        if os.path.isdir(os.path.join(self.path_lib, fullname.replace(".", os.sep))):
            return None

        raise ImportError(
            "'%s' is not declared by this test module -- add it to coverage or depend in define.yaml" % fullname.replace(".", "/")
        )


####################################################################################################################################
def cfg_parse(arg_list):
    """Parse the command line."""

    parser = argparse.ArgumentParser(prog="python.py", description="Run a python test module.")
    parser.add_argument("--lib", required=True, metavar="PATH", help="path of the harness library")
    parser.add_argument("--test", required=True, metavar="PATH", help="path of the test module")
    parser.add_argument("--allow", default="", metavar="LIST", help="comma separated library modules the test may import")
    parser.add_argument("--name", default="", metavar="LIST", help="comma separated tests to run, or all of them when not set")
    parser.add_argument("--coverage", metavar="PATH", help="measure coverage and write it as json to this path")

    return parser.parse_args(arg_list)


####################################################################################################################################
def coverage_begin(path_lib):
    """Start measuring coverage, returning the measurement or None when it cannot be measured.

    Measuring starts before the test module is loaded, since a module imports the code it covers and the definitions in that code
    run at import time."""

    try:
        import coverage
    except ImportError:
        print("unable to measure coverage: no module named 'coverage'")
        print("HINT: install python3-coverage (d12, u24, rh10) or py3-coverage (a321, a324), else pip3 install 'coverage>=6.5'")

        return None

    # Branch detail per line arrived in 6.5. An older one reports only summary counts, which cannot say which line to fix.
    if tuple(int(part) for part in coverage.__version__.split(".")[:2]) < (6, 5):
        print("coverage %s is too old, 6.5 or newer is required for branch detail" % coverage.__version__)
        print("HINT: the version packaged for rh9 is too old, so pip3 install 'coverage>=6.5' there")

        return None

    result = coverage.Coverage(branch=True, data_file=None, source=[path_lib])
    result.start()

    return result


####################################################################################################################################
def test_module_run(config):
    """Load the test module by path and run it with only the library modules it declared importable."""

    sys.path.insert(0, config.lib)
    sys.meta_path.insert(0, ImportGuard(config.lib, filter(None, config.allow.split(","))))

    measure = None

    if config.coverage is not None:
        measure = coverage_begin(config.lib)

        if measure is None:
            return 2

    spec = importlib.util.spec_from_file_location("test_module", config.test)
    test_module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(test_module)

    # Imported here rather than at the top so the guard above is installed before the harness is loaded
    from harness.test import test_run

    result = 0 if test_run(vars(test_module), list(filter(None, config.name.split(",")))) else 1

    if measure is not None:
        measure.stop()
        measure.json_report(outfile=config.coverage)

    return result


####################################################################################################################################
def main():
    """Run the test module named on the command line."""

    # All output goes to stdout and no bytecode is cached, for the same reasons as test.py
    sys.stderr = sys.stdout
    sys.dont_write_bytecode = True

    return test_module_run(cfg_parse(sys.argv[1:]))


####################################################################################################################################
# This is the program rather than an import, so it cannot run in the interpreter that is running the tests
if __name__ == "__main__":  # {uncoverable_branch}
    sys.exit(main())  # {uncoverable}
