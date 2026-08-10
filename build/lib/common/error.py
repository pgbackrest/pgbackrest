"""Error Handling.

Each tool reports failures with ToolError, which carries the exit status to return. Exit status mirrors the C: 0 is success, 1 means
a top-level coverage check found modules missing coverage, and anything greater is an error.

Anything else that gets out of a tool is a bug in the tool, which error_trace() renders the way the C renders a stack trace: enough
to find the bug without burying the message that says what it was."""

####################################################################################################################################
import traceback

# Exit status for an error. Distinct from 1, which a coverage command uses to report incomplete coverage rather than a failure.
EXIT_ERROR = 2

# Exit status when a signal terminated the tool, the same status the C exits with for TermError
EXIT_TERM = 63

# Innermost frames of a stack trace to show when it is trimmed
TRACE_FRAME_MAX = 6


####################################################################################################################################
class ToolError(Exception):
    """A harness error with the exit status to return."""

    def __init__(self, message, status=EXIT_ERROR):
        super().__init__(message)
        self.status = status


####################################################################################################################################
def error_trace(error, full=False):
    """Render an unexpected exception and its stack trace.

    An unexpected exception is a bug in the tool rather than a problem with what the tool was working on, so the stack trace is what
    gets it fixed. Only the innermost frames are shown, since the frames above them are the path from main() into the tool and are
    long enough to bury the message that says what went wrong.

    The complete trace, including any exception this one was raised while handling, is rendered when full is set, which is what the
    tools do at debug level as the C shows a stack trace only at debug level."""

    frame_list = traceback.extract_tb(error.__traceback__)

    # There is nothing to trim when the whole trace is already short enough, so render it as python does
    if full or len(frame_list) <= TRACE_FRAME_MAX:
        return "".join(traceback.format_exception(type(error), error, error.__traceback__)).rstrip("\n")

    # The message comes first so it is what gets read, followed by the frames in the order python reports them
    return (
        "".join(traceback.format_exception_only(type(error), error)).rstrip("\n")
        + "\nstack trace (innermost %u of %u frames, --log-level=debug shows all of them):\n" % (TRACE_FRAME_MAX, len(frame_list))
        + "".join(traceback.format_list(frame_list[-TRACE_FRAME_MAX:])).rstrip("\n")
    )


####################################################################################################################################
def check(condition, message):
    """Raise ToolError when a condition does not hold.

    Used for invariants a tool itself must maintain, i.e. a failure means the tool or the definitions it reads are wrong rather than
    the code it is working on."""

    if not condition:
        raise ToolError(message)
