"""Test Execute Process."""

####################################################################################################################################
import io
import time
from contextlib import redirect_stdout

from harness.test import *

from common.error import *
from common.exec import *


####################################################################################################################################
def test_exec_output():
    """The combined output is returned on success."""

    assert_equal(exec_one("echo hello"), "hello\n")

    # The command runs through the shell so shell syntax works
    assert_equal(exec_one("echo hello | tr a-z A-Z"), "HELLO\n")

    # Output written to stderr is folded in, which is where meson and ninja report what went wrong
    assert_equal(exec_one("echo out; echo err 1>&2"), "out\nerr\n")


####################################################################################################################################
def test_exec_error():
    """An unexpected exit status is an error that carries the output."""

    with assert_raises(ToolError) as error:
        exec_one("echo bad && false")

    assert_equal(str(error.exception), "echo bad && false terminated unexpectedly [1]: bad")

    # A command that wrote nothing reports only the status
    with assert_raises(ToolError) as error:
        exec_one("exit 7")

    assert_equal(str(error.exception), "exit 7 terminated unexpectedly [7]")


####################################################################################################################################
def test_exec_expect():
    """A status other than zero can be the expected one, which then makes zero unexpected."""

    assert_equal(exec_one("exit 3", 3), "")

    with assert_raises(ToolError) as error:
        exec_one("true", 3)

    assert_equal(str(error.exception), "true terminated unexpectedly [0]")


####################################################################################################################################
def test_exec_status():
    """A command that is expected to fail reports its status rather than raising."""

    assert_equal(exec_status("true"), 0)
    assert_equal(exec_status("exit 9"), 9)


####################################################################################################################################
def test_exec_async():
    """A command runs in the background and what it wrote to each stream is kept apart."""

    process = Exec("echo out; echo err 1>&2")
    process.begin()

    assert_equal(process.end(), 0)
    assert_equal(process.output, "out\n")

    # Anything on stderr means the command failed whatever its status, so it is reported separately
    assert_equal(process.error, "err\n")


####################################################################################################################################
def test_exec_async_poll():
    """A command that is still running reports no status, which is how the driver keeps every vm busy."""

    process = Exec("sleep 0.2; echo done")
    process.begin()

    poll = 0

    while process.end(wait=False) is None:
        poll += 1
        time.sleep(0.01)

    # The command was polled at least once while it was still running
    assert_true(poll > 0)
    assert_equal(process.output, "done\n")


####################################################################################################################################
def test_exec_async_show():
    """Output is written as it arrives, a line at a time, so a long command shows progress."""

    # The first write has no line ending so there is nothing to show until the rest of the line arrives
    process = Exec("printf partial; sleep 0.2; echo ' line'; echo next", show_output=True)
    output = io.StringIO()

    with redirect_stdout(output):
        process.begin()

        assert_equal(process.end(), 0)

    assert_equal(output.getvalue(), "    partial line\n    next\n")
    assert_equal(process.output, "partial line\nnext\n")


####################################################################################################################################
def test_exec_result():
    """Output and error are kept apart, since what a command wrote to stderr is not what it wrote."""

    status, output, error = exec_result("echo out; echo err >&2", suppress_stderr=True)

    assert_equal(status, 0)
    assert_equal(output, "out\n")
    assert_equal(error, "err\n")

    # Anything on stderr is a failure unless the caller is expecting it
    with assert_raises(ToolError) as raised:
        exec_result("echo out; echo err >&2")

    assert_in("output found on STDERR:\nerr", str(raised.exception))


####################################################################################################################################
def test_exec_result_status():
    """A command that fails is reported with what it wrote, unless it was expected to fail."""

    with assert_raises(ToolError) as raised:
        exec_result("echo out; echo err >&2; exit 3")

    assert_in("command 'echo out; echo err >&2; exit 3' returned 3\nSTDOUT:\nout\nSTDERR:\nerr", str(raised.exception))

    # A command the documentation is showing the failure of ends the way it was expected to, and what it wrote to stderr is the
    # error being shown rather than something that went wrong
    assert_equal(exec_result("exit 3", status_expect=3)[0], 3)
    assert_equal(exec_result("echo out; echo err >&2; exit 3", status_expect=3), (3, "out\n", "err\n"))

    with assert_raises(ToolError) as raised:
        exec_result("exit 1", status_expect=3)

    assert_in("returned 1, but 3 was expected", str(raised.exception))

    # A command whose failure does not matter
    assert_equal(exec_result("echo out; exit 1", suppress_error=True), (1, "out\n", ""))


####################################################################################################################################
def test_exec_result_retry():
    """A command worth retrying is tried until it works or until there is no time left to wait."""

    # Long enough to be tried more than once and short enough not to hold up the tests
    with assert_raises(ToolError) as raised:
        exec_result("exit 1", retry=1)

    assert_in("returned 1", str(raised.exception))

    # A command that works is not retried
    assert_equal(exec_result("echo out", retry=1)[1], "out\n")
