/***********************************************************************************************************************************
Expression Evaluation
***********************************************************************************************************************************/
#include <build.h>

#include <stdlib.h>
#include <string.h>

#include "command/build/eval.h"
#include "common/debug.h"

/***********************************************************************************************************************************
Numeric comparison operator types
***********************************************************************************************************************************/
typedef enum
{
    evalOpTypeGt,
    evalOpTypeGte,
    evalOpTypeLt,
    evalOpTypeLte,
} EvalOpType;

/***********************************************************************************************************************************
Find an operator at the top level (not inside parentheses)
***********************************************************************************************************************************/
static const char *
evalFindOp(const char *const expr, const char *const op)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRINGZ, expr);
        FUNCTION_TEST_PARAM(STRINGZ, op);
    FUNCTION_TEST_END();

    int depth = 0;
    const char *result = NULL;

    for (const char *pos = expr; *pos != '\0'; pos++)
    {
        if (*pos == '(')
            depth++;
        else if (*pos == ')')
            depth--;
        else if (depth == 0 && strncmp(pos, op, strlen(op)) == 0)
        {
            result = pos;
            break;
        }
    }

    FUNCTION_TEST_RETURN_TYPE_P(const char, result);
}

/***********************************************************************************************************************************
Evaluate a single expression (no &&/|| operators)
***********************************************************************************************************************************/
static bool
evalOne(const String *const expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    bool result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const trimmed = strTrim(strDup(expression));
        const char *const expr = strZ(trimmed);

        // Parenthesized expression
        if (expr[0] == '(')
        {
            CHECK_FMT(
                FormatError, strSize(trimmed) >= 2 && expr[strSize(trimmed) - 1] == ')',
                "unmatched parenthesis in '%s'", strZ(expression));

            result = eval(strNewZN(expr + 1, strSize(trimmed) - 2));
        }
        // Negation
        else if (expr[0] == '!')
        {
            result = !evalOne(strNewZ(expr + 1));
        }
        // String comparison with eq/ne
        else if (strstr(expr, " eq ") != NULL || strstr(expr, " ne ") != NULL)
        {
            const char *eqOp = strstr(expr, " eq ");
            const char *neOp = strstr(expr, " ne ");
            const bool isEq = eqOp != NULL;
            const char *const op = isEq ? eqOp : neOp;
            const size_t opLen = 4;

            // Extract left operand (strip surrounding quotes)
            const String *left = strTrim(strNewZN(expr, (size_t)(op - expr)));

            CHECK_FMT(
                FormatError, strSize(left) >= 2 && strZ(left)[0] == '\'' && strZ(left)[strSize(left) - 1] == '\'',
                "expected quoted string on left side of '%s': '%s'", isEq ? "eq" : "ne", strZ(expression));

            left = strNewZN(strZ(left) + 1, strSize(left) - 2);

            // Extract right operand (strip surrounding quotes)
            const String *right = strTrim(strNewZ(op + opLen));

            CHECK_FMT(
                FormatError, strSize(right) >= 2 && strZ(right)[0] == '\'' && strZ(right)[strSize(right) - 1] == '\'',
                "expected quoted string on right side of '%s': '%s'", isEq ? "eq" : "ne", strZ(expression));

            right = strNewZN(strZ(right) + 1, strSize(right) - 2);

            result = isEq ? strEq(left, right) : !strEq(left, right);
        }
        // Numeric comparison
        else if (strstr(expr, " >= ") != NULL || strstr(expr, " <= ") != NULL || strstr(expr, " > ") != NULL ||
                 strstr(expr, " < ") != NULL)
        {
            const char *op;
            size_t opLen;
            EvalOpType opType;

            if ((op = strstr(expr, " >= ")) != NULL)
            {
                opLen = 4;
                opType = evalOpTypeGte;
            }
            else if ((op = strstr(expr, " <= ")) != NULL)
            {
                opLen = 4;
                opType = evalOpTypeLte;
            }
            else if ((op = strstr(expr, " > ")) != NULL)
            {
                opLen = 3;
                opType = evalOpTypeGt;
            }
            else
            {
                op = strstr(expr, " < ");
                opLen = 3;
                opType = evalOpTypeLt;
            }

            const int left = atoi(expr);
            const int right = atoi(op + opLen);

            switch (opType)
            {
                case evalOpTypeGt:
                    result = left > right;
                    break;

                case evalOpTypeGte:
                    result = left >= right;
                    break;

                case evalOpTypeLt:
                    result = left < right;
                    break;

                case evalOpTypeLte:
                    result = left <= right;
                    break;
            }
        }
        else
            CHECK_FMT(FormatError, false, "unable to evaluate '%s'", strZ(expression));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}

/**********************************************************************************************************************************/
FN_EXTERN bool
eval(const String *const expression)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(STRING, expression);
    FUNCTION_TEST_END();

    ASSERT(expression != NULL);
    CHECK_FMT(AssertError, strstr(strZ(expression), "{[") == NULL, "unreplaced var in expression '%s'", strZ(expression));

    bool result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const char *const expr = strZ(expression);

        // Split on || (lowest precedence, left to right, skip parens)
        const char *const orOp = evalFindOp(expr, " || ");

        if (orOp != NULL)
        {
            result = eval(strTrim(strNewZN(expr, (size_t)(orOp - expr)))) ||
                     eval(strTrim(strNewZ(orOp + 4)));
        }
        else
        {
            // Split on && (next precedence, left to right, skip parens)
            const char *const andOp = evalFindOp(expr, " && ");

            if (andOp != NULL)
            {
                result = eval(strTrim(strNewZN(expr, (size_t)(andOp - expr)))) &&
                         eval(strTrim(strNewZ(andOp + 4)));
            }
            else
            {
                result = evalOne(expression);
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_TEST_RETURN(BOOL, result);
}
