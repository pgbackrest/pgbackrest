"""Test Harness.

Support for the harness unit tests, the equivalent of test/src/harness/test.c for the C tests. It is always importable so it does
not need to be declared in define.yaml, and it is not part of the harness itself so it is not covered.

A test is a module level function named test_*. Running and reporting is unittest's, so a failure is formatted the same way it
always was, but the tests themselves are plain functions with no class and no self.

The assertions are unittest's own, rebound as plain functions. A bare assert would report only that it failed and not the values,
since that introspection comes from pytest rather than from python."""

####################################################################################################################################
import sys
import unittest

# Assertions are instance methods so they are taken from an instance that is never run
_case = unittest.TestCase()

# Comparison
assert_equal = _case.assertEqual
assert_not_equal = _case.assertNotEqual

# Truth
assert_true = _case.assertTrue
assert_false = _case.assertFalse

# Identity and membership
assert_is_none = _case.assertIsNone
assert_is_not_none = _case.assertIsNotNone
assert_is_instance = _case.assertIsInstance
assert_in = _case.assertIn
assert_not_in = _case.assertNotIn

# Errors, used as a context manager, e.g. with assert_raises(ToolError) as error:
assert_raises = _case.assertRaises

# What a test module gets from "from harness.test import *". Listing it here means a test module never has to update its import
# when an assertion is added, while leaving the names resolvable by an editor, which injecting them into the module would not.
__all__ = [
    "assert_equal",
    "assert_false",
    "assert_in",
    "assert_is_instance",
    "assert_is_none",
    "assert_is_not_none",
    "assert_not_equal",
    "assert_not_in",
    "assert_raises",
    "assert_true",
]


####################################################################################################################################
class _FunctionTest(unittest.FunctionTestCase):
    """A test function as a test case, reported by function name rather than by the class wrapping it."""

    ################################################################################################################################
    def __str__(self):
        """Report by function name rather than by the class wrapping it."""

        return self._testFunc.__name__

    ################################################################################################################################
    def id(self):
        """Report by function name rather than by the class wrapping it."""

        return self._testFunc.__name__

    ################################################################################################################################
    def shortDescription(self):
        """Suppress the description so the runner prints only the name."""

        return None


####################################################################################################################################
def test_run(namespace, name_list=None):
    """Run the test functions in a namespace, in the order they are defined, and return whether they all passed.

    The namespace is the test module, i.e. vars(module) from the runner. Passing name_list runs only those tests."""

    suite = unittest.TestSuite()

    for name, value in namespace.items():
        # A test is defined in the module rather than imported into it, which keeps an imported helper from being run as a test
        if not name.startswith("test_") or not callable(value) or getattr(value, "__module__", None) != namespace["__name__"]:
            continue

        if not name_list or name in name_list:
            suite.addTest(_FunctionTest(value))

    return unittest.TextTestRunner(stream=sys.stdout, verbosity=2).run(suite).wasSuccessful()
