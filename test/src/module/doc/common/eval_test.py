"""Test Expression Evaluation."""

####################################################################################################################################
from harness.test import *

from common.eval import *
from common.error import *


####################################################################################################################################
def test_eval_compare():
    """A comparison is between what two values hold rather than how they were written."""

    # Strings, which must be quoted so that a value holding a space is still one value
    assert_true(eval_expression("'y' eq 'y'"))
    assert_false(eval_expression("'y' eq 'n'"))
    assert_true(eval_expression("'y' ne 'n'"))
    assert_false(eval_expression("'y' ne 'y'"))

    # Numbers, in each of the comparisons
    assert_true(eval_expression("2 > 1"))
    assert_false(eval_expression("1 > 1"))
    assert_true(eval_expression("1 >= 1"))
    assert_false(eval_expression("1 >= 2"))
    assert_true(eval_expression("1 < 2"))
    assert_false(eval_expression("1 < 1"))
    assert_true(eval_expression("1 <= 1"))
    assert_false(eval_expression("2 <= 1"))


####################################################################################################################################
def test_eval_logic():
    """Or binds loosest, and each side is evaluated only when it is needed."""

    assert_true(eval_expression("'y' eq 'y' || 'y' eq 'n'"))
    assert_false(eval_expression("'y' eq 'n' || 'y' eq 'n'"))
    assert_true(eval_expression("'y' eq 'y' && 2 > 1"))
    assert_false(eval_expression("'y' eq 'y' && 1 > 2"))

    # Or binds loosest, so this is the or of a true value and an and rather than the and of an or and a value
    assert_true(eval_expression("'y' eq 'y' || 'y' eq 'n' && 'y' eq 'n'"))

    # Parentheses bind what they hold, and an operator inside them belongs to it
    assert_false(eval_expression("('y' eq 'y' || 'y' eq 'n') && 'y' eq 'n'"))
    assert_true(eval_expression("('y' eq 'y')"))

    # A side that would not evaluate is not evaluated when the other side decides the result
    assert_true(eval_expression("'y' eq 'y' || bogus"))
    assert_false(eval_expression("'y' eq 'n' && bogus"))

    # Negation, including of an expression in parentheses
    assert_true(eval_expression("!'y' eq 'n'"))
    assert_false(eval_expression("!('y' eq 'y' || 'y' eq 'n')"))


####################################################################################################################################
def test_eval_error():
    """An expression that cannot be evaluated is reported with the expression, since that is what needs fixing."""

    # A variable that was never replaced, which would otherwise be compared as the text of its name
    with assert_raises(ToolError) as error:
        eval_expression("{[debug]} eq 'y'")

    assert_equal(str(error.exception), "unreplaced variable in expression '{[debug]} eq 'y''")

    with assert_raises(ToolError) as error:
        eval_expression("bogus")

    assert_equal(str(error.exception), "unable to evaluate 'bogus'")

    with assert_raises(ToolError) as error:
        eval_expression("('y' eq 'y'")

    assert_equal(str(error.exception), "unmatched parenthesis in '('y' eq 'y''")

    # A comparison against something that is not a quoted string, on either side
    with assert_raises(ToolError) as error:
        eval_expression("y eq 'y'")

    assert_equal(str(error.exception), "expected quoted string on left side of 'eq': 'y eq 'y''")

    with assert_raises(ToolError) as error:
        eval_expression("'y' ne y")

    assert_equal(str(error.exception), "expected quoted string on right side of 'ne': ''y' ne y'")

    # A comparison against something that is not a number
    with assert_raises(ToolError) as error:
        eval_expression("1 > y")

    assert_equal(str(error.exception), "expected number in '1 > y'")
