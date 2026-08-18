"""Block Macro Linter.

Checks that a block macro is closed by the END macro that matches it, and that a macro named like a block macro is classified as
one or as not one.

The compiler cannot check the pairing because several closers are aliases: MEM_CONTEXT_PRIOR_END(), MEM_CONTEXT_OBJ_END(), and
TEST_ERROR_MEM_CONTEXT_END() are each defined as the base MEM_CONTEXT_END(), and OBJ_NEW_END() as MEM_CONTEXT_NEW_END(). Closing
with the wrong one expands identically and builds clean, so nothing but a check like this reports it. Eight sites had drifted to the
base closer when it was first run.

The pairing is what the headers document as intended rather than what the preprocessor allows, which is why the check is worth
having even though every mismatch it has found was a no-op once expanded."""

####################################################################################################################################
from command.lint.lex import lex
from common.error import ToolError
from common.log import *

# Macros that open a block, each with the one macro that may close it. Several openers share a closer and several closers are an
# alias for another, so this cannot be derived from the names and is read out of the headers instead. Generating the closers with
# _BEGIN -> _END produced 57 errors that were not errors: OBJ_NEW_END() closes all four OBJ_NEW_*_BEGIN() variants, and
# MEM_CONTEXT_TEMP_RESET_BEGIN() closes with MEM_CONTEXT_TEMP_END().
_BLOCK_MACRO = {
    "FUNCTION_TEST_BEGIN": "FUNCTION_TEST_END",
    "FUNCTION_LOG_BEGIN": "FUNCTION_LOG_END",
    "MEM_CONTEXT_BEGIN": "MEM_CONTEXT_END",
    "MEM_CONTEXT_PRIOR_BEGIN": "MEM_CONTEXT_PRIOR_END",
    "MEM_CONTEXT_OBJ_BEGIN": "MEM_CONTEXT_OBJ_END",
    "MEM_CONTEXT_NEW_BEGIN": "MEM_CONTEXT_NEW_END",
    "MEM_CONTEXT_TEMP_BEGIN": "MEM_CONTEXT_TEMP_END",
    "MEM_CONTEXT_TEMP_RESET_BEGIN": "MEM_CONTEXT_TEMP_END",
    "OBJ_NEW_BEGIN": "OBJ_NEW_END",
    "OBJ_NEW_BASE_BEGIN": "OBJ_NEW_END",
    "OBJ_NEW_EXTRA_BEGIN": "OBJ_NEW_END",
    "OBJ_NEW_BASE_EXTRA_BEGIN": "OBJ_NEW_END",
    "TRY_BEGIN": "TRY_END",
    "YAML_MAP_BEGIN": "YAML_MAP_END",
    "YAML_SEQ_BEGIN": "YAML_SEQ_END",
    "YAML_ITER_BEGIN": "YAML_ITER_END",
    # HRN_FORK_CHILD_BEGIN() opens a do block and an if block and its END closes both, so the nesting still balances one for one
    "HRN_FORK_BEGIN": "HRN_FORK_END",
    "HRN_FORK_CHILD_BEGIN": "HRN_FORK_CHILD_END",
    "HRN_FORK_PARENT_BEGIN": "HRN_FORK_PARENT_END",
    "TEST_ERROR_MEM_CONTEXT_BEGIN": "TEST_ERROR_MEM_CONTEXT_END",
}

# Macros named like a block macro that open no block, so they take no part in the nesting. They are still a BEGIN/END pair by
# convention, but whether they are used in pairs is a different check from whether a block is closed correctly and is not made here.
_BLOCK_MACRO_SKIP_LIST = {
    "COMMENT_BLOCK_BEGIN",  # A string used to render a comment banner
    "COMMENT_BLOCK_END",
    "FUNCTION_TEST_MEM_CONTEXT_AUDIT_BEGIN",  # Expands inside FUNCTION_TEST_BEGIN() rather than opening anything itself
    "FUNCTION_TEST_MEM_CONTEXT_AUDIT_END",
    # Reads as an exact parallel to FUNCTION_TEST_BEGIN(), which opens "if (stackTraceTest()) {", but is a different shape: this
    # pushes the stack trace and returns, and its END expands to nothing at all. A macro is classified by what it expands to.
    "FUNCTION_HARNESS_BEGIN",
    "FUNCTION_HARNESS_END",
    "INFO_CHECKSUM_BEGIN",  # A do block that is closed in the same macro
    "INFO_CHECKSUM_END",
    "BENCHMARK_BEGIN",  # A list of declarations and statements, defined inside a function
    "BENCHMARK_END",
}

# Macros that may close a block, and every macro that has been classified as opening one, closing one, or neither
_BLOCK_MACRO_CLOSER = set(_BLOCK_MACRO.values())
_BLOCK_MACRO_KNOWN = _BLOCK_MACRO.keys() | _BLOCK_MACRO_CLOSER | _BLOCK_MACRO_SKIP_LIST


####################################################################################################################################
def _lint_macro_pair(token_list):
    """Check that each block macro is closed by the macro that matches it and return the number of errors found."""

    result = 0
    open_list = []

    for token in token_list:
        # Only an identifier in code can open or close a block, since a macro is not code where it is defined
        if token.kind != "identifier" or token.directive:
            continue

        if token.text in _BLOCK_MACRO:
            open_list.append(token)
        elif token.text in _BLOCK_MACRO_CLOSER:
            if not open_list:
                log(WARN, "line %u: %s() closes a block that was never opened" % (token.line, token.text))
                result += 1

                continue

            open_token = open_list.pop()
            expected = _BLOCK_MACRO[open_token.text]

            if token.text != expected:
                log(
                    WARN,
                    "line %u: %s() opened on line %u is closed with %s() rather than %s()"
                    % (token.line, open_token.text, open_token.line, token.text, expected),
                )

                result += 1

    # What is left open at the end of the file is never closed, reported innermost first as the block that needs closing
    for token in reversed(open_list):
        log(WARN, "line %u: %s() is never closed with %s()" % (token.line, token.text, _BLOCK_MACRO[token.text]))
        result += 1

    return result


####################################################################################################################################
def _lint_macro_define(token_list):
    """Check that a macro named like a block macro is classified and return the number of errors found.

    A block macro added with a closer that is not in the table above would otherwise pass silently: neither the opener nor the
    closer is known, both are skipped, and the pairing check has nothing to say. Requiring the definition to be classified is what
    closes that hole. It found 14 macros that had been missed by hand, including the whole HRN_FORK_* family, which was 172 pairs
    going unchecked while the check still reported nothing."""

    result = 0

    for index, token in enumerate(token_list):
        # Look for "#define NAME" in a directive, i.e. the macro is defined here rather than used
        if not token.directive or token.text != "#" or index + 2 >= len(token_list) or token_list[index + 1].text != "define":
            continue

        name = token_list[index + 2]

        if name.kind == "identifier" and name.text.endswith(("_BEGIN", "_END")) and name.text not in _BLOCK_MACRO_KNOWN:
            log(WARN, "line %u: %s() is not classified in test/lib/command/lint/macro.py" % (name.line, name.text))
            result += 1

    return result


####################################################################################################################################
def lint_macro(source):
    """Check the block macros in a source file and return the number of errors found."""

    # Source the lexer cannot read is one error and nothing more can be said about it, since the checks below read tokens
    try:
        token_list = list(lex(source))
    except ToolError as error:
        log(WARN, str(error))

        return 1

    return _lint_macro_pair(token_list) + _lint_macro_define(token_list)
