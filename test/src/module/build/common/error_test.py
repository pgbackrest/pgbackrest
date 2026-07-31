"""Test Error Handling."""

####################################################################################################################################
from harness.test import *

from common.error import *


####################################################################################################################################
def test_error_message():
    """The message is what gets reported and the status is what the harness exits with."""

    error = ToolError("something went wrong")

    assert_equal(str(error), "something went wrong")
    assert_equal(error.status, EXIT_ERROR)

    # Ordinary handlers must be able to catch it
    assert_is_instance(error, Exception)


####################################################################################################################################
def test_error_status():
    """A status other than the default is kept.

    That is how the coverage command reports incomplete coverage rather than a failure."""

    assert_equal(ToolError("incomplete", 1).status, 1)
    assert_equal(ToolError("failed", 0).status, 0)

    # The default is distinct from the incomplete coverage status so the two are never confused
    assert_not_equal(EXIT_ERROR, 1)

    # A run that was terminated by a signal is distinct from one that failed
    assert_not_equal(EXIT_TERM, EXIT_ERROR)
    assert_not_equal(EXIT_TERM, 1)


####################################################################################################################################
def _raise_nested(depth):
    """Raise from depth nested calls."""

    if depth > 0:
        _raise_nested(depth - 1)

    raise ValueError("something unexpected")


####################################################################################################################################
def _error_nested(depth):
    """Return an exception raised from depth nested calls, so its trace has depth + 2 frames.

    The exception is caught here rather than with assert_raises(), which clears the traceback the test needs."""

    try:
        _raise_nested(depth)
    except ValueError as error:
        return error


####################################################################################################################################
def test_error_trace_trim():
    """A trace with more frames than the limit keeps the innermost of them and says how many there were."""

    error = _error_nested(TRACE_FRAME_MAX)
    trace = error_trace(error)

    # The message comes first so a trace cannot bury it, and the frame count says what is missing and how to get it
    assert_true(trace.startswith("ValueError: something unexpected\n"))
    assert_in("stack trace (innermost %u of %u frames" % (TRACE_FRAME_MAX, TRACE_FRAME_MAX + 2), trace)
    assert_in("--log-level=debug", trace)

    # The frames kept are the innermost ones, so the trace is shorter than the complete one and ends where the error was raised
    assert_true(len(trace.splitlines()) < len(error_trace(error, True).splitlines()))
    assert_true(trace.endswith('raise ValueError("something unexpected")'))


####################################################################################################################################
def test_error_trace_full():
    """The complete trace is python's own, which is what debug level gets and what a trace short enough to keep gets anyway."""

    trace = error_trace(_error_nested(TRACE_FRAME_MAX), True)

    assert_true(trace.startswith("Traceback (most recent call last):\n"))
    assert_true(trace.endswith("ValueError: something unexpected"))

    # A trace with no more frames than the limit is not trimmed, so there is nothing to say about what was dropped
    trace = error_trace(_error_nested(0))

    assert_true(trace.startswith("Traceback (most recent call last):\n"))
    assert_not_in("innermost", trace)


####################################################################################################################################
def test_check_pass():
    """A condition that holds passes silently."""

    for condition in (True, 1, "text", [0], {"key": "value"}):
        assert_is_none(check(condition, "should not raise"), condition)


####################################################################################################################################
def test_check_fail():
    """A condition that does not hold raises with the message given."""

    for condition in (False, 0, "", [], {}, None):
        with assert_raises(ToolError) as error:
            check(condition, "invariant does not hold")

        assert_equal(str(error.exception), "invariant does not hold")
        assert_equal(error.exception.status, EXIT_ERROR)
