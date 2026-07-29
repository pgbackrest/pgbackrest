"""Error Handling.

The harness reports failures with TestError, which carries the exit status to return. Exit status mirrors the C harness: 0 is
success, 1 means a top-level coverage check found modules missing coverage, and anything greater is an error."""

####################################################################################################################################
# Exit status for an error. Distinct from 1, which a coverage command uses to report incomplete coverage rather than a failure.
EXIT_ERROR = 2


####################################################################################################################################
class TestError(Exception):
    """A harness error with the exit status to return."""

    def __init__(self, message, status=EXIT_ERROR):
        super().__init__(message)
        self.status = status


####################################################################################################################################
def check(condition, message):
    """Raise TestError when a condition does not hold.

    Used for invariants the harness itself must maintain, i.e. a failure means the harness or the definitions it reads are wrong
    rather than the code under test."""

    if not condition:
        raise TestError(message)
