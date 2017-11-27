# pgBackRest <br/> Coding Standards

## Standards

### Indentation

Indentation is four spaces -- no tabs. Only file types that absolutely require tabs (e.g. `Makefile`) may use them.

### Naming

#### Variables

Variable names use camel case with the first letter lower-case.

- `stanzaName` - the name of the stanza

- `nameIdx` - loop variable for iterating through a list of names

Variable names should be descriptive. Avoid `i`, `j`, etc.

#### Types

Type names use camel case with the first letter upper case:

`typedef struct MemContext <...>`

`typedef enum {<...>} ErrorState;`

#### Constants

**#define Constants**

`#define` constants should be all caps with `_` separators.
```
#DEFINE MY_CONSTANT "STRING"
```
The value should be aligned at column 69 whenever possible.

This type of constant should mostly be used for strings. Use enums whenever possible for integer constants.

**Enum Constants**

Enum elements follow the same case rules as variables. They are strongly typed so this shouldn't present any confusion.
```
typedef enum
{
    cipherModeEncrypt,
    cipherModeDecrypt,
} CipherMode;
```
Note the comma after the last element. This reduces diff churn when new elements are added.

#### Macros

Macro names should be upper-case with underscores between words. Macros (except simple constants) should be avoided whenever possible as they make code less clear and test coverage harder to measure.

Macros should follow the format:
```
#define MACRO(paramName1, paramName2)   \
    <code>
```
If the macro defines a block it should look like:
```
#define MACRO_2(paramName1, paramName2) \
{                                       \
    <code>                              \
}
```
Continuation characters should be aligned at column 132 (unlike the examples above that have been shortened for display purposes).

To avoid conflicts, variables in a macro will be named `[macro name]_[var name]`, e.g. `TEST_RESULT_resultExpected`. Variables that need to be accessed in wrapped code should be provided accessor macros.

#### Begin / End

Use `Begin` / `End` for names rather than `Start` / `Finish`, etc.

#### New / Free

Use `New` / `Free` for constructors and destructors rather than `Create` / `Destroy`, etc.

### Formatting

#### Braces

C allows braces to be excluded for a single statement. However, braces should be used when the control statement (if, while, etc.) spans more than one line or the statement to be executed spans more than one line.

No braces needed:
```
if (condition)
    return value;
```
Braces needed:
```
if (conditionThatUsesEntireLine1 &&
    conditionThatUsesEntireLine2)
{
    return value;
}
```
```
if (condition)
{
    return
        valueThatUsesEntireLine1 &&
        valueThatUsesEntireLine2;
}
```

## Language Elements

### Data Types

Don't get exotic - use the simplest type that will work.

Use `int` or `unsigned int` for general cases. `int` will be at least 32 bits. When not using `int` use one of the types defined in `common/type.h`.

### Macros

Don't use a macro when a function could be used instead. Macros make it hard to measure code coverage.

### Objects

Object-oriented programming is used extensively. The object pointer is always referred to as `this`.

## Testing

### Uncoverable/Uncovered Code

#### Uncoverable Code

The `uncoverable` keyword marks code that can never be covered. For instance, a function that never returns because it always throws a error. Uncoverable code should be rare to non-existent outside the common libraries and test code.
```
}   // {uncoverable - function throws error so never returns}
```
Subsequent code that is uncoverable for the same reason is marked with `// {+uncoverable}`.

#### Uncovered Code

Marks code that is not tested for one reason or another. This should be kept to a minimum and an excuse given for each instance.
```
exit(EXIT_FAILURE); // {uncoverable - test harness does not support non-zero exit}
```
Subsequent code that is uncovered for the same reason is marked with `// {+uncovered}`.
