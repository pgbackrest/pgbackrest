"""Console Logging.

Format matches src/common/log.c: an optional timestamp ("YYYY-MM-DD HH:MM:SS.mmm "), the process id ("P00"), and the log level
right-aligned to six characters followed by ": ". Lines after the first in a multi-line message are indented to the prefix width.

The level is passed to log() rather than being named by the function, as it is in the Perl code, so there is one function to find
rather than one per level.

All output goes to stdout. The Perl test framework treats anything a test writes to stderr as a failure, so nothing here may go
there, including errors."""

####################################################################################################################################
import sys
import time

# Log levels, using the same values as LogLevel in src/common/logLevel.h
OFF = 0
ERROR = 2
WARN = 3
INFO = 4
DETAIL = 5
DEBUG = 6
TRACE = 7

# Level names as they appear on the command line and in the log
LEVEL_NAME = {
    OFF: "off",
    ERROR: "error",
    WARN: "warn",
    INFO: "info",
    DETAIL: "detail",
    DEBUG: "debug",
    TRACE: "trace",
}

LEVEL_ID = {name: level for level, name in LEVEL_NAME.items()}

# Current settings, replaced by log_init()
_level = INFO
_timestamp = True


####################################################################################################################################
def log_init(level, timestamp):
    """Set the log level and whether timestamps are shown.

    Timestamps are suppressed when generating documentation so the output is reproducible."""

    global _level, _timestamp

    _level = level
    _timestamp = timestamp


####################################################################################################################################
def log_level_parse(name):
    """Convert a level name to its id, or None when the name is not valid."""

    return LEVEL_ID.get(name)


####################################################################################################################################
def log_level_enum(level):
    """The C enum name for a level, e.g. "logLevelDebug", used for the test.c substitution."""

    name = LEVEL_NAME[level]

    return "logLevel" + name[0].upper() + name[1:]


####################################################################################################################################
def log(level, message):
    """Write a message at the specified level.

    Continuation lines are indented to line up under the first line."""

    if level > _level:
        return

    prefix = ""

    if _timestamp:
        now = time.time()
        prefix = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(now)) + ".%03d " % (int(now * 1000) % 1000)

    prefix += "P00 %6s: " % LEVEL_NAME[level].upper()

    sys.stdout.write(prefix + str(message).replace("\n", "\n" + " " * len(prefix)) + "\n")
    sys.stdout.flush()
