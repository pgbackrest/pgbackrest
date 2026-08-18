"""C Lexer.

Turns C source into tokens so a linter can check what the source says rather than what a regular expression can find in it.

Parsing would be the obvious way to do this and does not work here. The block macros are brace-unbalanced by design, e.g.
FUNCTION_TEST_BEGIN() ends on an open brace that FUNCTION_TEST_END() closes and MEM_CONTEXT_TEMP_BEGIN()/END() wrap "do {" and
"} while (0)", so a C file in this project is not valid C as written and only balances once it has been preprocessed. A parser that
reads the source as written reads MEM_CONTEXT_TEMP_BEGIN() as a call and puts everything in the block at the wrong depth, and since
such a parser is error tolerant it does not fail but quietly produces a tree that is wrong. A parser that reads preprocessed source
never sees a macro at all, since every one worth checking has been expanded by then. A lexer has neither problem: a macro is just an
identifier and the rule reading the tokens supplies what it means."""

####################################################################################################################################
import bisect
import re

from common.error import ToolError

# Punctuation, which is C99 less the digraphs since the project does not use them. The longest is given first so it is matched
# before the punctuation it is made of, e.g. "<<=" is one token rather than "<<" followed by "=".
_PUNCT_MULTI = "... <<= >>= -> ++ -- << >> <= >= == != && || *= /= %= += -= &= ^= |= ##"
_PUNCT_SINGLE = "[ ] ( ) { } . & * + - ~ ! / % < > ^ | ? : ; = , #"

# Token patterns, tried in the order they are given. A comment or a literal must be matched before punctuation, since "/" and the
# quotes are punctuation as well and what is inside them is not code.
_TOKEN_LIST = (
    ("comment", r"/\*.*?\*/|//[^\n]*"),
    ("string", r'"(?:\\.|[^"\\\n])*"'),
    ("char", r"'(?:\\.|[^'\\\n])*'"),
    # A preprocessing number, e.g. 0x1p-3, 1.5f, 0xFFUL. An exponent is matched before the digits and letters it is made of, since
    # matching the letter on its own would leave the sign to be read as an operator.
    ("number", r"\.?[0-9](?:[eEpP][+-]|[0-9a-zA-Z_.])*"),
    ("identifier", r"[A-Za-z_][A-Za-z0-9_]*"),
    ("newline", r"\n"),
    ("space", r"[ \t]+"),
    ("punct", "|".join(re.escape(punct) for punct in (_PUNCT_MULTI + " " + _PUNCT_SINGLE).split())),
)

# Dot matches a newline so a block comment can span lines
_TOKEN_EXP = re.compile("|".join("(?P<%s>%s)" % (kind, pattern) for kind, pattern in _TOKEN_LIST), re.DOTALL)


####################################################################################################################################
class Token:
    """A token, with the line it is on and whether it is part of a preprocessor directive.

    A rule that reads code skips the directives, since a macro is not code where it is defined, e.g. a block macro is defined with
    the brace it opens left open. There is an object per token, so it holds only what a rule reads."""

    __slots__ = ("kind", "text", "line", "directive")

    def __init__(self, kind, text, line, directive):
        self.kind = kind
        self.text = text
        self.line = line
        self.directive = directive


####################################################################################################################################
def _splice(source):
    r"""Remove each backslash-newline, returning the source and the offsets a newline was removed at.

    This is translation phase 2 and has to happen before tokenizing in phase 3 rather than being folded into the lexer as a token
    kind, since a splice may fall in the middle of any token: the generated help data wraps a compressed blob at the line length and
    splits the octal escape \374 across the break as "\3" and "74". The offsets are what the line a token is on is recovered from,
    since every newline before it that was removed here has to be added back."""

    result = []
    removed = []
    pos = 0
    size = 0

    while True:
        index = source.find("\\\n", pos)

        # The rest of the source has no splice in it, so it is the last piece
        if index < 0:
            result.append(source[pos:])
            break

        chunk = source[pos:index]
        result.append(chunk)
        size += len(chunk)
        removed.append(size)
        pos = index + 2

    return "".join(result), removed


####################################################################################################################################
def lex(source):
    """Yield the tokens in C source, with the line each one is on.

    Raises ToolError on a character that is not part of any token, since a rule cannot say anything useful about source the lexer
    did not understand. Space and newlines are not yielded, so a rule sees code rather than layout."""

    source, removed = _splice(source)
    size = len(source)
    pos = 0
    line = 1
    line_start = True
    directive = False

    while pos < size:
        match = _TOKEN_EXP.match(source, pos)

        # Report what could not be read up to the end of the line, so the message says enough to find it without repeating the file
        if match is None:
            raise ToolError(
                "line %u: cannot lex '%s'" % (line + bisect.bisect_right(removed, pos), source[pos : pos + 20].split("\n")[0])
            )

        kind = match.lastgroup
        text = match.group()

        # A directive runs to the end of the line it is on, which is a line the splice above has already joined its continuations to
        if kind == "newline":
            directive = False
            line_start = True
        elif kind != "space":
            if line_start and text == "#":
                directive = True

            line_start = False

            # Add back the newlines that splicing removed before this point to get the line in the source as written
            yield Token(kind, text, line + bisect.bisect_right(removed, pos), directive)

        # Count the newlines in the token so a block comment does not put everything after it on the line it began on
        line += text.count("\n")
        pos = match.end()
