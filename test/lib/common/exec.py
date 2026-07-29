"""Execute Process.

Commands run through the shell with stderr folded into stdout, matching execOneExpectP() in src/build/common/exec.c. The combined
output is returned on success and carried in the error on failure, since a failed meson or ninja run says what went wrong there
rather than in its exit status."""

####################################################################################################################################
import subprocess

from common.error import TestError


####################################################################################################################################
def exec_one(command, result_expect=0):
    """Run a command through the shell and return its combined output.

    Raise TestError when the exit status is not the expected one."""

    process = subprocess.run(
        ["sh", "-c", command + " 2>&1"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True
    )

    if process.returncode != result_expect:
        output = process.stdout.strip()

        raise TestError("%s terminated unexpectedly [%d]%s%s" % (command, process.returncode, ": " if output else "", output))

    return process.stdout
