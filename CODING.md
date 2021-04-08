# pgBackRest <br/> Coding Standards

## Standards

### Indentation

Indentation is four spaces -- no tabs. Only file types that absolutely require tabs (e.g. `Makefile`) may use them.

### Line Length

With the exception of documentation code, no line of any code or test file shall exceed 132 characters. If a line break is required, then it shall be after the first function parenthesis:
```
// CORRECT - location of line break after first function parenthesis if line length is greater than 132
StringList *removeList = infoBackupDataLabelList(
    infoBackup, strNewFmt("^%s.*", strZ(strLstGet(currentBackupList, fullIdx))));

// INCORRECT
StringList *removeList = infoBackupDataLabelList(infoBackup, strNewFmt("^%s.*", strZ(strLstGet(currentBackupList,
    fullIdx))));
```
If a conditional, then after a completed conditional, for example:
```
// CORRECT - location of line break after a completed conditional if line length is greater than 132
if (archiveInfoPgHistory.id != backupInfoPgHistory.id ||
    archiveInfoPgHistory.systemId != backupInfoPgHistory.systemId ||
    archiveInfoPgHistory.version != backupInfoPgHistory.version)

// INCORRECT
if (archiveInfoPgHistory.id != backupInfoPgHistory.id || archiveInfoPgHistory.systemId !=
    backupInfoPgHistory.systemId || archiveInfoPgHistory.version != backupInfoPgHistory.version)
```

### Function Comments

Comments for `extern` functions should be included in the `.h` file. Comments for `static` functions and implementation-specific notes for `extern` functions (i.e., not of interest to the general user) should be included in the `.c` file.

### Inline Comment

Inline comments shall start at character 69 and must not exceed the line length of 132. For example:
```
typedef struct InlineCommentExample
{
    const String *comment;                                          // Inline comment example
    const String *longComment;                                      // Inline comment example that exceeds 132 characters should
                                                                    // then go to next line but this should be avoided
} InlineCommentExample;
```

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
```c
#define MY_CONSTANT                                                 "STRING"
```
The value should be aligned at column 69 whenever possible.

This type of constant should mostly be used for strings. Use enums whenever possible for integer constants.

**String Constants**

String constants can be declared using the `STRING_STATIC()` macro for local strings and `STRING_EXTERN()` for strings that will be extern'd for use in other modules.

Extern'd strings should be declared in the header file as:
```c
#define SAMPLE_VALUE                                                "STRING"
    STRING_DECLARE(SAMPLE_VALUE_STR);
```
And in the C file as:
```c
STRING_EXTERN(SAMPLE_VALUE_STR,                                     SAMPLE_VALUE);
```
Static strings declared in the C file are not required to have a `#define` if the `#define` version is not used. Extern'd strings must always have the `#define` in the header file.

**Enum Constants**

Enum elements follow the same case rules as variables. They are strongly typed so this shouldn't present any confusion.
```c
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
```c
#define MACRO(paramName1, paramName2)   \
    <code>
```
If the macro defines a block it should look like:
```c
#define MACRO_2(paramName1, paramName2) \
{                                       \
    <code>                              \
}
```
Continuation characters should be aligned at column 132 (unlike the examples above that have been shortened for display purposes).

To avoid conflicts, variables in a macro will be named `[macro name]_[var name]`, e.g. `TEST_RESULT_resultExpected`. Variables that need to be accessed in wrapped code should be provided accessor macros.

[Variadic functions](#variadic-functions) are an exception to the capitalization rule.

#### Begin / End

Use `Begin` / `End` for names rather than `Start` / `Finish`, etc.

#### New / Free

Use `New` / `Free` for constructors and destructors rather than `Create` / `Destroy`, etc.

### Formatting

#### Braces

C allows braces to be excluded for a single statement. However, braces should be used when the control statement (if, while, etc.) spans more than one line or the statement to be executed spans more than one line.

No braces needed:
```c
if (condition)
    return value;
```
Braces needed:
```c
if (conditionThatUsesEntireLine1 &&
    conditionThatUsesEntireLine2)
{
    return value;
}
```
```c
if (condition)
{
    return
        valueThatUsesEntireLine1 &&
        valueThatUsesEntireLine2;
}
```
Braces should be added to `switch` statement cases that have a significant amount of code. As a general rule of thumb, if the code block in the `case` is large enough to have blank lines and/or multiple comments then it should be enclosed in braces.
```c
switch (int)
{
    case 1:
        a = 2;
        break;

    case 2:
    {
        # Comment this more complex code
        a = 1;
        b = 2;

        c = func(a, b);

        break;
    }
}
```

#### Hints, Warnings, and Errors

Hints are to be formatted with capitalized `HINT:` followed by a space and a sentence. The sentence shall only begin with a capital letter if the first word is an acronym (e.g. TLS) or a proper name (e.g. PostgreSQL). The sentence must end with a period, question mark or exclamation point as appropriate.

Warning and errors shall be lowercase with the exceptions for proper names and acronyms and end without punctuation.

## Language Elements

### Data Types

Don't get exotic - use the simplest type that will work.

Use `int` or `unsigned int` for general cases. `int` will be at least 32 bits. When not using `int` use one of the types defined in `common/type.h`.

### Macros

Don't use a macro when a function could be used instead. Macros make it hard to measure code coverage.

### Objects

Object-oriented programming is used extensively. The object pointer is always referred to as `this`.

An object can expose internal struct members by defining a public struct that contains the members to be exposed and using inline functions to get/set the members.

The header file:
```c
/***********************************************************************************************************************************
Getters/setters
***********************************************************************************************************************************/
typedef struct ListPub
{
    unsigned int listSize;                                          // List size
} ListPub;

// List size
__attribute__((always_inline)) static inline unsigned int
lstSize(const List *this)
{
    ASSERT_INLINE(this != NULL);
    return ((const ListPub *)this)->listSize;
}
```
And the C file:
```c
struct List
{
    ListPub pub;                                                    // Publicly accessible variables
    ...
};
```
The public struct must be the first member of the private struct. The naming convention for the public struct is to add `Pub` to the end of the private struct name.

### Variadic Functions

Variadic functions can take a variable number of parameters. While the `printf()` pattern is variadic, it is not very flexible in terms of optional parameters given in any order.

This project implements variadic functions using macros (which are exempt from the normal macro rule of being all caps). A typical variadic function definition:
```c
typedef struct StoragePathCreateParam
{
    bool errorOnExists;
    bool noParentCreate;
    mode_t mode;
} StoragePathCreateParam;

#define storagePathCreateP(this, pathExp, ...)                              \
    storagePathCreate(this, pathExp, (StoragePathCreateParam){__VA_ARGS__})
#define storagePathCreateP(this, pathExp)                                  \
    storagePathCreate(this, pathExp, (StoragePathCreateParam){0})

void storagePathCreate(const Storage *this, const String *pathExp, StoragePathCreateParam param);
```
Continuation characters should be aligned at column 132 (unlike the example above that has been shortened for display purposes).

This function can be called without variable parameters:
```c
storagePathCreateP(storageLocal(), "/tmp/pgbackrest");
```
Or with variable parameters:
```c
storagePathCreateP(storageLocal(), "/tmp/pgbackrest", .errorOnExists = true, .mode = 0777);
```
If the majority of functions in a module or object are variadic it is best to provide macros for all functions even if they do not have variable parameters. Do not use the base function when variadic macros exist.

## Testing

### Uncoverable/Uncovered Code

#### Uncoverable Code

The `uncoverable` keyword marks code that can never be covered. For instance, a function that never returns because it always throws an error. Uncoverable code should be rare to non-existent outside the common libraries and test code.
```c
}   // {uncoverable - function throws error so never returns}
```
Subsequent code that is uncoverable for the same reason is marked with `// {+uncoverable}`.

#### Uncovered Code

Marks code that is not tested for one reason or another. This should be kept to a minimum and an excuse given for each instance.
```c
exit(EXIT_FAILURE); // {uncovered - test harness does not support non-zero exit}
```
Subsequent code that is uncovered for the same reason is marked with `// {+uncovered}`.
