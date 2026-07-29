"""Test Execute Process."""

####################################################################################################################################
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

    with assert_raises(TestError) as error:
        exec_one("echo bad && false")

    assert_equal(str(error.exception), "echo bad && false terminated unexpectedly [1]: bad")

    # A command that wrote nothing reports only the status
    with assert_raises(TestError) as error:
        exec_one("exit 7")

    assert_equal(str(error.exception), "exit 7 terminated unexpectedly [7]")


####################################################################################################################################
def test_exec_expect():
    """A status other than zero can be the expected one, which then makes zero unexpected."""

    assert_equal(exec_one("exit 3", 3), "")

    with assert_raises(TestError) as error:
        exec_one("true", 3)

    assert_equal(str(error.exception), "true terminated unexpectedly [0]")
