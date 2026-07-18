#!/usr/bin/env python3
####################################################################################################################################
# pgBackRest Smoke Test
#
# A minimal, dependency-free (Python standard library only) sanity check that a built pgbackrest binary actually works. It finds
# every PostgreSQL installation on the system, using the same path list as the test harness (test/src/harness/host.c), and drives
# pgbackrest through a full backup/restore cycle against each version found:
#
#   initdb -> configure -> stanza-create -> check -> full backup -> load data -> incr backup -> wipe -> restore -> verify
#
# The test uses a local POSIX (filesystem) repository and a private unix socket so it never collides with a running PostgreSQL or an
# existing pgbackrest installation. It is a build-sanity check for packagers, not a functional test suite.
#
# Exit status: 0 = every supported version found was tested and passed, 1 = a version failed or nothing could be tested. Testing
# nothing is a failure rather than a skip, since a test that silently does nothing is not a successful test.
####################################################################################################################################
import argparse
import glob
import os
import pwd
import re
import shutil
import subprocess
import sys
import tempfile
import time

# Stanza name used throughout the test
STANZA = "smoke"

# PostgreSQL port. There is no TCP listener (listen_addresses is empty) so this only names the socket file in the private socket
# directory and never collides with a system PostgreSQL.
PG_PORT = 5432

# Per-command timeout (seconds). The clusters are tiny so every step is fast; this only guards against a hang.
COMMAND_TIMEOUT = 300


class SmokeError(Exception):
    """A smoke test step failed."""


####################################################################################################################################
# Command runner that optionally drops privileges (when the script runs as root)
####################################################################################################################################
class Runner:
    def __init__(self, verbose, uid=None, gid=None, user=None):
        self.verbose = verbose
        self.uid = uid  # target uid when dropping privileges, else None
        self.gid = gid
        self.user = user  # OS user commands run as (also the PostgreSQL superuser)

    # Return a preexec function that drops to the target user, or None when running as the current user
    def _preexec(self):
        if self.uid is None:
            return None

        uid, gid, user = self.uid, self.gid, self.user

        def demote():
            os.setgid(gid)
            os.initgroups(user, gid)
            os.setuid(uid)

        return demote

    # Build a clean environment for child processes
    def _env(self, home, bin_path):
        env = {
            "HOME": home,
            "PATH": bin_path + os.pathsep + os.environ.get("PATH", "/usr/bin:/bin"),
            "LC_ALL": "C",
            "LANG": "C",
        }

        return env

    # Run a command, capturing combined output. Raise SmokeError on failure or timeout.
    def run(self, cmd, home, bin_path, input_text=None):
        if self.verbose:
            sys.stderr.write("+ " + " ".join(cmd) + "\n")
            sys.stderr.flush()

        try:
            result = subprocess.run(
                cmd,
                cwd=home,
                env=self._env(home, bin_path),
                preexec_fn=self._preexec(),
                input=input_text,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                timeout=COMMAND_TIMEOUT,
            )
        except subprocess.TimeoutExpired:
            raise SmokeError("timed out after %ds: %s" % (COMMAND_TIMEOUT, " ".join(cmd)))

        if self.verbose and result.stdout:
            sys.stderr.write(result.stdout)
            sys.stderr.flush()

        if result.returncode != 0:
            raise SmokeError("command failed (exit %d): %s\n%s" % (result.returncode, " ".join(cmd), result.stdout))

        return result.stdout

    # Create a directory and hand ownership to the target user when dropping privileges
    def make_dir(self, path, mode=0o700):
        os.makedirs(path, mode=mode, exist_ok=True)

        if self.uid is not None:
            os.chown(path, self.uid, self.gid)

    # Recursively chown a tree to the target user (used after creating the work dir skeleton as root)
    def chown_tree(self, path):
        if self.uid is None:
            return

        os.chown(path, self.uid, self.gid)

        for root, dirs, files in os.walk(path):
            for name in dirs + files:
                os.chown(os.path.join(root, name), self.uid, self.gid)


####################################################################################################################################
# Discover PostgreSQL installations
####################################################################################################################################
# Candidate bin directories, mirroring the list in test/src/harness/host.c
def pg_bin_candidates(extra_dirs):
    candidates = []
    candidates += sorted(glob.glob("/usr/lib/postgresql/*/bin"))  # Debian
    candidates += sorted(glob.glob("/usr/pgsql-*/bin"))  # RHEL (PGDG)
    candidates += sorted(glob.glob("/usr/libexec/postgresql*"))  # Alpine
    candidates += ["/usr/bin"]  # Native (e.g. RHEL application stream)
    candidates += list(extra_dirs)

    return candidates


# Read the major version from a bin directory, or None if it is not a usable PostgreSQL install
def pg_version(bin_dir):
    for tool in ("initdb", "pg_ctl", "psql"):
        if not os.path.isfile(os.path.join(bin_dir, tool)) or not os.access(os.path.join(bin_dir, tool), os.X_OK):
            return None

    try:
        output = subprocess.check_output(
            [os.path.join(bin_dir, "pg_ctl"), "--version"], universal_newlines=True, stderr=subprocess.STDOUT
        )
    except (subprocess.CalledProcessError, OSError):
        return None

    # e.g. "pg_ctl (PostgreSQL) 16.2" or "pg_ctl (PostgreSQL) 9.6.24". The version is encoded as major * 100 + minor so it sorts and
    # compares naturally: 9.6 -> 906, 10 -> 1000, 16 -> 1600. For version 10 and later the major is a single number and the trailing
    # component is only the patch release, so it is ignored.
    match = re.search(r"\)\s+(\d+)(?:\.(\d+))?", output)

    if match is None:
        return None

    major = int(match.group(1))

    if major >= 10:
        return major * 100

    # Versions before 10 have a two-part major version (e.g. 9.6)
    if match.group(2) is None:
        return None

    return major * 100 + int(match.group(2))


# Read the full version string from a bin directory, e.g. "pg_ctl (PostgreSQL) 19beta2". This is only called for an install
# returned by find_postgres(), where pg_version() has already run pg_ctl successfully.
def pg_version_full(bin_dir):
    output = subprocess.check_output(
        [os.path.join(bin_dir, "pg_ctl"), "--version"], universal_newlines=True, stderr=subprocess.STDOUT
    )

    return output.strip()


# Return a sorted list of (version, bin_dir) and a sorted list of versions skipped because pgBackRest does not support them. The
# installs are deduplicated by the real path of initdb so a symlinked install is not run twice.
def find_postgres(extra_dirs, only_versions, supported):
    found = {}
    skipped = set()

    for bin_dir in pg_bin_candidates(extra_dirs):
        if not os.path.isdir(bin_dir):
            continue

        version = pg_version(bin_dir)

        if version is None:
            continue

        # Skip versions that pgBackRest does not support, e.g. an old PostgreSQL that happens to be installed
        if supported is not None and version not in supported:
            skipped.add(version)
            continue

        if only_versions and version not in only_versions:
            continue

        key = os.path.realpath(os.path.join(bin_dir, "initdb"))

        if key not in found:
            found[key] = (version, bin_dir)

    return sorted(found.values()), sorted(skipped)


# Parse a version string (e.g. "9.6" or "16") into the encoded form used internally
def version_parse(version):
    if "." in version:
        major, minor = version.split(".", 1)

        return int(major) * 100 + int(minor)

    return int(version) * 100


# Read the PostgreSQL versions supported by pgBackRest from the generated version header. The header is generated from
# src/build/postgres/postgres.yaml and, unlike that file, is shipped in the distribution, so this works from a source checkout or a
# distribution tarball. Return None when the header cannot be found so that every version found is tested.
def supported_versions():
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "postgres", "version.auto.h")

    try:
        with open(path) as file:
            content = file.read()
    except OSError:
        return None

    result = {version_parse(match) for match in re.findall(r'^#define PG_VERSION_\w+_Z\s+"([0-9.]+)"', content, re.MULTILINE)}

    return result or None


# Render a version number for display (e.g. 906 -> "9.6", 1600 -> "16")
def version_str(version):
    major = version // 100
    minor = version % 100

    if major >= 10:
        return str(major)

    return "%d.%d" % (major, minor)


####################################################################################################################################
# Run the smoke test for a single PostgreSQL version
####################################################################################################################################
def smoke_one(pgbackrest, version, bin_dir, runner, keep):
    work = tempfile.mkdtemp(prefix="pgbackrest-smoke-", dir=temp_base())
    data = os.path.join(work, "data")
    repo = os.path.join(work, "repo")
    log = os.path.join(work, "log")
    socket = os.path.join(work, "socket")
    lock = os.path.join(work, "lock")
    config = os.path.join(work, "pgbackrest.conf")
    pg_log = os.path.join(log, "postgresql.log")

    # Helpers scoped to this run
    def pg(cmd):
        return runner.run([os.path.join(bin_dir, cmd[0])] + cmd[1:], work, bin_dir)

    def pgbr(args):
        return runner.run([pgbackrest, "--config=" + config, "--stanza=" + STANZA] + args, work, bin_dir)

    def psql(sql):
        return runner.run(
            [
                os.path.join(bin_dir, "psql"),
                "-h",
                socket,
                "-p",
                str(PG_PORT),
                "-U",
                runner.user,
                "-d",
                "postgres",
                "-v",
                "ON_ERROR_STOP=1",
                "-tAqc",
                sql,
            ],
            work,
            bin_dir,
        )

    pg_started = False
    success = False

    try:
        # Pre-create the directories the tools do not create themselves and hand the whole tree to the run user
        for path in (socket, log, lock):
            os.makedirs(path, mode=0o700, exist_ok=True)

        runner.chown_tree(work)

        # Initialize the cluster (checksums on, no fsync since it is throwaway, trust auth for local connections)
        pg(["initdb", "-k", "-N", "--pgdata=" + data, "--auth=trust"])

        # Append test configuration to postgresql.conf
        conf = (
            "\n"
            "unix_socket_directories = '%s'\n"
            "listen_addresses = ''\n"
            "port = %d\n"
            "wal_level = replica\n"
            "archive_mode = on\n"
            "archive_command = '%s --config=%s --stanza=%s archive-push %%p'\n"
            "fsync = off\n" % (socket, PG_PORT, pgbackrest, config, STANZA)
        )
        append_file(os.path.join(data, "postgresql.conf"), conf)

        # Write the pgbackrest configuration
        write_file(
            config,
            "[global]\n"
            "repo1-path=%s\n"
            "log-path=%s\n"
            "lock-path=%s\n"
            "log-level-console=detail\n"
            "log-level-file=off\n"
            "start-fast=y\n"
            "compress-type=lz4\n"
            "\n"
            "[%s]\n"
            "pg1-path=%s\n"
            "pg1-port=%d\n"
            "pg1-socket-path=%s\n"
            "pg1-user=%s\n" % (repo, log, lock, STANZA, data, PG_PORT, socket, runner.user),
            runner,
        )

        # Start PostgreSQL
        pg(["pg_ctl", "start", "-D", data, "-l", pg_log, "-w"])
        pg_started = True

        # Load data
        psql("CREATE TABLE smoke AS SELECT generate_series(1, 1000) AS id")

        # Create the stanza and confirm archiving works end to end
        pgbr(["stanza-create"])
        pgbr(["check"])

        # Full backup
        pgbr(["--type=full", "backup"])

        # Add more data and force the WAL segment to archive
        psql("INSERT INTO smoke SELECT generate_series(1001, 2000)")
        psql("SELECT %s()" % ("pg_switch_wal" if version >= 1000 else "pg_switch_xlog"))

        # Incremental backup. The backup command waits for the required WAL to archive, so the restore below has no race.
        pgbr(["--type=incr", "backup"])

        expected = 2000

        # Stop PostgreSQL and remove the data directory to prove the restore rebuilds it from the repository
        pg(["pg_ctl", "stop", "-D", data, "-w", "-m", "fast"])
        pg_started = False
        shutil.rmtree(data)
        runner.make_dir(data, mode=0o700)

        # Restore the latest backup (recovery config is written automatically) and start PostgreSQL
        pgbr(["restore"])
        pg(["pg_ctl", "start", "-D", data, "-l", pg_log, "-w"])
        pg_started = True

        # Wait for recovery to replay all WAL and promote (there is no standby.signal so PostgreSQL promotes at end of WAL)
        wait_for_promote(psql)

        # Verify the restored data matches what was backed up
        actual = int(psql("SELECT count(*) FROM smoke").strip())

        if actual != expected:
            raise SmokeError("restored row count %d does not match expected %d" % (actual, expected))

        # Show backup info (printed by the runner when verbose)
        pgbr(["info"])

        pg(["pg_ctl", "stop", "-D", data, "-w", "-m", "fast"])
        pg_started = False
        success = True
    finally:
        # Always try to stop PostgreSQL so no stray processes are left behind
        if pg_started:
            try:
                runner.run([os.path.join(bin_dir, "pg_ctl"), "stop", "-D", data, "-w", "-m", "immediate"], work, bin_dir)
            except SmokeError:
                pass

        # Remove the work directory on success, else report where it was kept so the failure can be investigated
        if success and not keep:
            shutil.rmtree(work, ignore_errors=True)
        else:
            sys.stderr.write("work directory kept: %s\n" % work)


# Poll until the cluster has finished recovery and accepts connections
def wait_for_promote(psql):
    deadline = time.time() + 60

    while time.time() < deadline:
        try:
            if psql("SELECT pg_is_in_recovery()").strip() == "f":
                return
        except SmokeError:
            pass  # server still starting or rejecting during recovery

        time.sleep(0.5)

    raise SmokeError("cluster did not finish recovery within 60s")


####################################################################################################################################
# Small file and path helpers
####################################################################################################################################
# Prefer /tmp for the work directory to keep unix socket paths short (macOS TMPDIR can exceed the socket path limit)
def temp_base():
    if os.path.isdir("/tmp") and os.access("/tmp", os.W_OK):
        return "/tmp"

    return tempfile.gettempdir()


def write_file(path, contents, runner):
    with open(path, "w") as file:
        file.write(contents)

    if runner.uid is not None:
        os.chown(path, runner.uid, runner.gid)


# No ownership change is needed here since the file being appended to was created by initdb as the run user
def append_file(path, contents):
    with open(path, "a") as file:
        file.write(contents)


####################################################################################################################################
# Resolve the user to run as, dropping privileges when running as root
####################################################################################################################################
def resolve_runner(verbose, user_name):
    # Not root: run everything as the current user
    if os.geteuid() != 0:
        return Runner(verbose, user=pwd.getpwuid(os.geteuid()).pw_name)

    # Root: PostgreSQL refuses to run as root, so drop to an unprivileged user
    try:
        info = pwd.getpwnam(user_name)
    except KeyError:
        return None

    return Runner(verbose, uid=info.pw_uid, gid=info.pw_gid, user=info.pw_name)


####################################################################################################################################
# Read the machine architecture and endianness from an ELF binary. This confirms which architecture is actually being exercised,
# which matters when the binary is built for a foreign architecture and run under emulation.
####################################################################################################################################
def binary_arch(path):
    # ELF e_machine values for the architectures pgBackRest is built on
    machines = {0x03: "i386", 0x3E: "x86_64", 0x15: "ppc64", 0x16: "s390x", 0xB7: "aarch64", 0x28: "arm"}

    with open(path, "rb") as file:
        header = file.read(20)

    if len(header) < 20 or header[:4] != b"\x7fELF":
        return "unknown (not an ELF binary)"

    little = header[5] == 1
    machine = int.from_bytes(header[18:20], "little" if little else "big")

    return "%s (%s-bit %s-endian)" % (
        machines.get(machine, "machine 0x%02X" % machine),
        "64" if header[4] == 2 else "32",
        "little" if little else "big",
    )


####################################################################################################################################
# Main
####################################################################################################################################
def main():
    parser = argparse.ArgumentParser(description="Run pgBackRest smoke tests against installed PostgreSQL versions.")
    parser.add_argument("--pgbackrest", required=True, help="path to the pgbackrest binary to test")
    parser.add_argument("--pg-bin-path", action="append", default=[], metavar="DIR", help="extra PostgreSQL bin directory")
    parser.add_argument("--version", action="append", default=[], metavar="MAJOR", help="only run this major version (repeatable)")
    parser.add_argument("--user", default="postgres", help="unprivileged user to drop to when run as root (default: postgres)")
    parser.add_argument("--keep", action="store_true", help="do not delete the work directory")
    parser.add_argument("-v", "--verbose", action="store_true", help="echo commands and their output")
    arg = parser.parse_args()

    pgbackrest = os.path.realpath(arg.pgbackrest)

    if not os.path.isfile(pgbackrest) or not os.access(pgbackrest, os.X_OK):
        sys.stderr.write("pgbackrest binary not found or not executable: %s\n" % pgbackrest)
        return 1

    # Extra bin dirs from the command line and the environment (colon-separated)
    extra_dirs = list(arg.pg_bin_path)
    env_dirs = os.environ.get("PGBACKREST_SMOKE_PG_BIN_PATH", "")

    if env_dirs:
        extra_dirs += env_dirs.split(os.pathsep)

    # Parse the requested version filter (accepts "9.6" or "16")
    try:
        only_versions = {version_parse(item) for item in arg.version}
    except ValueError:
        sys.stderr.write("invalid --version: expected a major version such as 9.6 or 16\n")
        return 1

    supported = supported_versions()
    installs, unsupported = find_postgres(extra_dirs, only_versions, supported)

    # Report any installs that were skipped because pgBackRest does not support them
    if unsupported:
        sys.stderr.write("skipped unsupported PostgreSQL: %s\n" % ", ".join(version_str(v) for v in unsupported))

    # Testing nothing is a failure. If there is no PostgreSQL to test against then the build has not been verified.
    if not installs:
        sys.stderr.write("no supported PostgreSQL installation found to test against\n")
        return 1

    runner = resolve_runner(arg.verbose, arg.user)

    if runner is None:
        sys.stderr.write("running as root but user '%s' does not exist so PostgreSQL cannot be run\n" % arg.user)
        return 1

    sys.stdout.write(
        "pgbackrest smoke test: %s\nrunning as: %s\narchitecture: %s\nversions supported: %s\nversions found: %s\n\n"
        % (
            pgbackrest,
            runner.user,
            binary_arch(pgbackrest),
            ", ".join(version_str(v) for v in sorted(supported)) if supported else "unknown",
            ", ".join(version_str(v) for v, _ in installs),
        )
    )

    passed = 0
    failed = 0

    for version, bin_dir in installs:
        label = "PostgreSQL %s (%s)" % (version_str(version), bin_dir)
        sys.stdout.write("=== %s ===\n" % label)

        # Log the version of PostgreSQL installed on the host. Only the major version is discovered above so this shows the minor
        # version, which is especially useful during the beta period.
        sys.stdout.write("%s\n" % pg_version_full(bin_dir))
        sys.stdout.flush()

        start = time.time()

        try:
            smoke_one(pgbackrest, version, bin_dir, runner, arg.keep)
            passed += 1
            sys.stdout.write("PASS %s (%.1fs)\n\n" % (label, time.time() - start))
        except SmokeError as e:
            failed += 1
            sys.stdout.write("FAIL %s (%.1fs)\n%s\n\n" % (label, time.time() - start, e))

        sys.stdout.flush()

    sys.stdout.write("summary: %d passed, %d failed\n" % (passed, failed))

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
