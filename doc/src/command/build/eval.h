/***********************************************************************************************************************************
Expression Evaluation

Simple expression evaluator that handles the subset of expressions used in doc XML if attributes.

Supports:
    - Negation: !expr
    - Parentheses: (expr)
    - String comparison: 'str1' eq 'str2', 'str1' ne 'str2'
    - Numeric comparison: num > num, num >= num, num < num, num <= num
    - Logical and: expr && expr
    - Logical or: expr || expr
***********************************************************************************************************************************/
#ifndef DOC_COMMAND_EVAL_H
#define DOC_COMMAND_EVAL_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Evaluate an expression and return the boolean result
FN_EXTERN bool eval(const String *expression);

#endif
