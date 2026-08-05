r"""ASCII Linter.

Checks that source is 7-bit ASCII, which keeps out characters that could be used to hide code, e.g. invisible, zero-width,
bidirectional, or homoglyph characters. Intentional non-ASCII byte values must be written as \xNN escapes so they remain visible in
the source."""

####################################################################################################################################
from common.log import *

# Bytes that may appear in source: printable 7-bit ASCII plus tab and linefeed
_BYTE_ALLOW = bytes(range(0x20, 0x7F)) + b"\t\n"


####################################################################################################################################
def lint_ascii(data):
    """Check that a file is 7-bit ASCII and return the number of errors found.

    Tab and newline are the only control characters permitted."""

    # Nothing to report unless a byte outside the allowed set is present. Checking first keeps the scan below off the hot path.
    if not data.translate(None, delete=_BYTE_ALLOW):
        return 0

    result = 0
    line = 1
    index = 0
    size = len(data)

    while index < size:
        char = data[index]

        # Count lines for reporting
        if char == 0x0A:
            line += 1
            index += 1

            continue

        # Tab is the only other permitted control character
        if char == 0x09 or 0x20 <= char <= 0x7E:
            index += 1

            continue

        # Decode the code point for reporting on a best-effort basis. Validation is unnecessary since the character is rejected
        # regardless, so the byte count is taken from the lead byte and limited to the bytes available.
        seq_size = 1

        if char >= 0xF0:
            seq_size = 4
        elif char >= 0xE0:
            seq_size = 3
        elif char >= 0xC0:
            seq_size = 2

        if seq_size > size - index:
            seq_size = size - index

        code_point = char

        if seq_size > 1:
            code_point = char & (0xFF >> (seq_size + 1))

            for seq_index in range(1, seq_size):
                code_point = (code_point << 6) | (data[index + seq_index] & 0x3F)

        log(WARN, "line %u contains disallowed character U+%04X (source must be 7-bit ASCII)" % (line, code_point))
        result += 1

        index += seq_size

    return result
