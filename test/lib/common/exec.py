"""Execute Process.

Exec runs a command in the background so several can run at once, which is how the test driver keeps every vm busy. It keeps stdout
and stderr apart because a test that writes anything to stderr has failed whatever its exit status, e.g. a valgrind error.

exec_one() runs a command and waits for it, with stderr folded into the output as execOneExpectP() in src/build/common/exec.c does.
The combined output is returned on success and carried in the error on failure, since a failed meson or ninja run says what went
wrong there rather than in its exit status."""

####################################################################################################################################
import os
import select
import subprocess
import sys

from common.error import TestError


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

    Raise TestError when the exit status is not the expected one."""

    process = Exec(command, show_output=show_output, merge_error=True)
    process.begin()
    status = process.end()

    if status != result_expect:
        output = process.output.strip()

        raise TestError("%s terminated unexpectedly [%d]%s%s" % (command, status, ": " if output else "", output))

    return process.output


####################################################################################################################################
def exec_status(command):
    """Run a command through the shell and return its exit status, discarding the output.

    Used where a command is expected to fail, e.g. pulling an image that is not in the cache."""

    process = Exec(command)
    process.begin()

    return process.end()
