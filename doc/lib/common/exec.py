"""Execute Process.

Exec runs a command in the background so several can run at once, which is how the test driver keeps every vm busy. It keeps stdout
and stderr apart because a test that writes anything to stderr has failed whatever its exit status, e.g. a valgrind error.

exec_one() runs a command and waits for it, with stderr folded into the output as execOneExpectP() in test/src/harness/exec.c does.
The combined output is returned on success and carried in the error on failure, since a failed meson or ninja run says what went
wrong there rather than in its exit status.

exec_result() keeps them apart instead, which is what the documentation build needs: what a command wrote is shown to a reader, and
what it wrote to stderr is not part of that. It also knows about commands that are meant to fail, since the documentation shows those
too, and about commands worth retrying, since a host that has just started may not be listening yet."""

####################################################################################################################################
import os
import select
import subprocess
import sys
import time

from common.error import ToolError

# How long to wait between attempts at a command that is being retried
_RETRY_SLEEP = 0.5


####################################################################################################################################
class Exec:
    """Run a command in the background and collect its output."""

    def __init__(self, command, show_output=False, merge_error=False):
        self.command = command
        self.show_output = show_output  # Write output as it arrives rather than only keeping it?
        self.merge_error = merge_error  # Fold stderr into the output rather than keeping it apart?
        self.output = ""
        self.error = ""

        self._process = None
        self._fd_out = None
        self._fd_list = []
        self._show_idx = 0  # How much of the output has been written when show_output is set

    ################################################################################################################################
    def begin(self):
        """Start the command."""

        self._process = subprocess.Popen(
            ["sh", "-c", self.command],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT if self.merge_error else subprocess.PIPE,
        )
        self._fd_out = self._process.stdout.fileno()
        self._fd_list = [self._fd_out] + ([] if self.merge_error else [self._process.stderr.fileno()])

    ################################################################################################################################
    def _show(self):
        """Write the complete lines that have arrived since the last call, indented the way the log indents output."""

        idx = self.output.rfind("\n") + 1

        if idx > self._show_idx:
            for line in self.output[self._show_idx : idx].splitlines():
                sys.stdout.write("    %s\n" % line)

            sys.stdout.flush()
            self._show_idx = idx

    ################################################################################################################################
    def _drain(self, timeout):
        """Read whatever is available on stdout and stderr, dropping a stream when it reaches end of file.

        A timeout of zero returns immediately and None waits for data, which is only safe once the command has exited."""

        for fd in select.select(self._fd_list, [], [], timeout)[0]:
            data = os.read(fd, 65536)

            # No data on a ready stream means end of file
            if not data:
                self._fd_list.remove(fd)

                continue

            if fd == self._fd_out:
                self.output += data.decode(errors="replace")

                if self.show_output:
                    self._show()
            else:
                self.error += data.decode(errors="replace")

    ################################################################################################################################
    def end(self, wait=True):
        """Return the exit status, or None when the command is still running and wait is not set.

        Output is drained while the command runs so a full pipe buffer cannot block it."""

        while self._process.poll() is None:
            self._drain(0.05 if wait else 0)

            if not wait:
                return None

        # The command is done so read what is left. Waiting for data is safe here since the only writer has exited.
        while self._fd_list:
            self._drain(None)

        self._process.stdout.close()

        if not self.merge_error:
            self._process.stderr.close()

        return self._process.returncode


####################################################################################################################################
def exec_one(command, result_expect=0, show_output=False):
    """Run a command through the shell and return its combined output.

    Raise ToolError when the exit status is not the expected one."""

    process = Exec(command, show_output=show_output, merge_error=True)
    process.begin()
    status = process.end()

    if status != result_expect:
        output = process.output.strip()

        raise ToolError("%s terminated unexpectedly [%d]%s%s" % (command, status, ": " if output else "", output))

    return process.output


####################################################################################################################################
def _exec_result(command, status_expect, suppress_error, suppress_stderr, show_output):
    """Run a command once and return the exit status, the output, and the error."""

    process = Exec(command, show_output=show_output)
    process.begin()
    status = process.end()

    # A command that failed the way it was expected to has ended the way it should. What it wrote to stderr is the error the
    # documentation is showing rather than something that went wrong, so it is not checked.
    if status_expect != 0 and status == status_expect:
        return status, process.output, process.error

    if status != status_expect:
        if not suppress_error:
            raise ToolError(
                "command '%s' returned %d%s\n%s%s"
                % (
                    command,
                    status,
                    "" if status_expect == 0 else ", but %d was expected" % status_expect,
                    "" if process.output == "" else "STDOUT:\n%s" % process.output,
                    "" if process.error == "" else "STDERR:\n%s" % process.error,
                )
            )
    elif process.error != "" and not suppress_stderr and not suppress_error:
        raise ToolError("STDOUT:\n%s\n\noutput found on STDERR:\n%s" % (process.output, process.error))

    return status, process.output, process.error


####################################################################################################################################
def exec_result(command, status_expect=0, suppress_error=False, suppress_stderr=False, retry=None, show_output=False):
    """Run a command through the shell, keeping its output and error apart, and check how it ended.

    A command that writes to stderr has failed whatever its exit status, unless it is a command whose errors are being shown. Retry
    keeps trying for that many seconds before letting the failure through, for a command that is waiting on something to be ready.
    """

    if retry is None:
        return _exec_result(command, status_expect, suppress_error, suppress_stderr, show_output)

    time_end = time.time() + retry

    while time.time() < time_end:
        try:
            return _exec_result(command, status_expect, suppress_error, suppress_stderr, show_output)
        except ToolError:
            time.sleep(_RETRY_SLEEP)

    # The last attempt is the one that reports the failure, since by now there is no time left to wait
    return _exec_result(command, status_expect, suppress_error, suppress_stderr, show_output)


####################################################################################################################################
def exec_status(command):
    """Run a command through the shell and return its exit status, discarding the output.

    Used where a command is expected to fail, e.g. pulling an image that is not in the cache."""

    process = Exec(command)
    process.begin()

    return process.end()
