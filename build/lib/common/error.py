"""Error Handling.

Each tool reports failures with ToolError, which carries the exit status to return. Exit status mirrors the C: 0 is success, 1 means a
top-level coverage check found modules missing coverage, and anything greater is an error."""

####################################################################################################################################
# Exit status for an error. Distinct from 1, which a coverage command uses to report incomplete coverage rather than a failure.
EXIT_ERROR = 2


####################################################################################################################################
class ToolError(Exception):
    """A harness error with the exit status to return."""

    def __init__(self, message, status=EXIT_ERROR):
        super().__init__(message)
        self.status = status


####################################################################################################################################
def check(condition, message):
    """Raise ToolError when a condition does not hold.

    Used for invariants a tool itself must maintain, i.e. a failure means the tool or the definitions it reads are wrong rather than
    the code it is working on."""

    if not condition:
        raise ToolError(message)
