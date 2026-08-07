"""Expression Evaluation.

Evaluates the expression an if attribute holds, which decides whether the node it is on stays in the document. Variables are
replaced before the expression gets here, so it works on values rather than on names.

The grammar is only what the documentation needs: or, and, parentheses, negation, string equality, and numeric comparison. Or binds
loosest and is split first, so "a || b && c" is "a || (b && c)"."""

####################################################################################################################################
from common.error import ToolError

# Numeric comparisons, longest operator first so that >= is not read as > followed by =
_COMPARE_LIST = (
    (" >= ", lambda left, right: left >= right),
    (" <= ", lambda left, right: left <= right),
    (" > ", lambda left, right: left > right),
    (" < ", lambda left, right: left < right),
)


####################################################################################################################################
def _find_op(expression, op):
    """Find an operator that is not inside parentheses, since one that is belongs to the expression they hold."""

    depth = 0

    for idx in range(len(expression)):
        if expression[idx] == "(":
            depth += 1
        elif expression[idx] == ")":
            depth -= 1
        elif depth == 0 and expression.startswith(op, idx):
            return idx

    return -1


####################################################################################################################################
def _quoted(value, side, op, expression):
    """The value a quoted string holds, since a comparison is between what was written rather than how it was written."""

    if len(value) < 2 or value[0] != "'" or value[-1] != "'":
        raise ToolError("expected quoted string on %s side of '%s': '%s'" % (side, op, expression))

    return value[1:-1]


####################################################################################################################################
def _number(value, expression):
    """The number a value holds."""

    try:
        return int(value.strip())
    except ValueError:
        raise ToolError("expected number in '%s'" % expression)


####################################################################################################################################
def _eval_one(expression):
    """Evaluate an expression that has no or/and in it."""

    trimmed = expression.strip()

    # An expression in parentheses is evaluated on its own
    if trimmed.startswith("("):
        if not trimmed.endswith(")"):
            raise ToolError("unmatched parenthesis in '%s'" % expression)

        return eval_expression(trimmed[1:-1])

    if trimmed.startswith("!"):
        return not _eval_one(trimmed[1:])

    # String comparison
    for op in ("eq", "ne"):
        idx = trimmed.find(" %s " % op)

        if idx != -1:
            left = _quoted(trimmed[:idx].strip(), "left", op, expression)
            right = _quoted(trimmed[idx + len(op) + 2 :].strip(), "right", op, expression)

            return left == right if op == "eq" else left != right

    # Numeric comparison
    for op, compare in _COMPARE_LIST:
        idx = trimmed.find(op)

        if idx != -1:
            return compare(_number(trimmed[:idx], expression), _number(trimmed[idx + len(op) :], expression))

    raise ToolError("unable to evaluate '%s'" % expression)


####################################################################################################################################
def eval_expression(expression):
    """Evaluate an expression, which must have had its variables replaced already."""

    if "{[" in expression:
        raise ToolError("unreplaced variable in expression '%s'" % expression)

    # Or binds loosest so it is split first, which leaves and to bind tighter than it. Each side is evaluated only when it is
    # needed, so an expression that is decided by its left side is not held to what is on its right.
    for op in (" || ", " && "):
        idx = _find_op(expression, op)

        if idx != -1:
            left = expression[:idx].strip()
            right = expression[idx + len(op) :].strip()

            if op == " || ":
                return eval_expression(left) or eval_expression(right)

            return eval_expression(left) and eval_expression(right)

    return _eval_one(expression)
