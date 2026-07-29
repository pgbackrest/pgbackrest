"""Test Error Handling."""

####################################################################################################################################
from harness.test import *

from common.error import *


####################################################################################################################################
def test_error_message():
    """The message is what gets reported and the status is what the harness exits with."""

    error = TestError("something went wrong")

    assert_equal(str(error), "something went wrong")
    assert_equal(error.status, EXIT_ERROR)

    # Ordinary handlers must be able to catch it
    assert_is_instance(error, Exception)


####################################################################################################################################
def test_error_status():
    """A status other than the default is kept.

    That is how the coverage command reports incomplete coverage rather than a failure."""

    assert_equal(TestError("incomplete", 1).status, 1)
    assert_equal(TestError("failed", 0).status, 0)

    # The default is distinct from the incomplete coverage status so the two are never confused
    assert_not_equal(EXIT_ERROR, 1)


####################################################################################################################################
def test_check_pass():
    """A condition that holds passes silently."""

    for condition in (True, 1, "text", [0], {"key": "value"}):
        assert_is_none(check(condition, "should not raise"), condition)


####################################################################################################################################
def test_check_fail():
    """A condition that does not hold raises with the message given."""

    for condition in (False, 0, "", [], {}, None):
        with assert_raises(TestError) as error:
            check(condition, "invariant does not hold")

        assert_equal(str(error.exception), "invariant does not hold")
        assert_equal(error.exception.status, EXIT_ERROR)
