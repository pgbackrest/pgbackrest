"""Line Length Linter.

Checks that no line is longer than the project line length. A line that runs past it wraps or scrolls out of view in a review, so
whatever is on the end of it does not get read."""

####################################################################################################################################
from common.log import *
from common.render import LINE_LENGTH


####################################################################################################################################
def lint_line(source):
    """Check the line lengths in a file and return the number of errors found."""

    result = 0

    for index, line in enumerate(source.split("\n")):
        if len(line) > LINE_LENGTH:
            log(WARN, "line %u is %u characters (maximum is %u)" % (index + 1, len(line), LINE_LENGTH))
            result += 1

    return result
