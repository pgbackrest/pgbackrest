 /**********************************************************************************************************************************
 A Doxygen filter which annotates comments in a C file.
     doxy_filter  <filename>

 The goal is to take an existing, non-doxygen comment style and turn it into doxygen comments.
 It is packaged as a filter. Given the file name of a C file, it updates the comments and sends output to stdout.

 The idea is to identify all comments in the code while keeping track of a small amount of context from
 around the comment.  The context allows us to determine:
    - Is the comment at the beginning of the file?
    - Is the comment at the global level in the file?
    - Is the comment inside a struct/union/enum?
    - Does the comment trail other statements? Or is it in front of other statements.

 Basically, the filter does the following:
     - A comment at the start of a file gets the @FILE tag added.
       The tag is necessary for doxygen to recognize C code.
       By convention, this first comment describes the overall purpose of the file.
    -  Doxygen comments are passed on through.
       Doxygen comments include "/*** ... /"  and "/// ..." style comments.
    -  Regular comments at the global level and inside structs/enums/unions are converted into doxygen comments.
       The assumption is these comments describe fields and entities like procedures, variables and macros.
    -  Trailing comments are associated with the previous item.
           "int Foo; // Comment about Foo"  becomes a doxygen comment describing Foo.

 The filter doesn't handle all existing doxygen comments. For example, it doesn't recognize the grouping tags
     "//{"  and "//}", nor does it passively echo other doxygen constructs.  Also, it has a very limited
     understanding of C syntax. For now, it is enough to get by.

 TODO? respect conditional compilation, add a File comment if none exists, be robust in not altering existing doxygen comments.
 **********************************************************************************************************************************/
%option yylineno
%{
#include <stdbool.h>
%}
    // Variables used to track the context around a comment.
    int lines = 0; // How many lines of code have been processed. Comment lines are not counted at the moment.
    int level = 0; // Level of nesting within brackets. Used to detect globals.
    bool trailing = false;  // Is there statement before the comment? If so, we are a trailing comment.
    bool complexData = false; // Are we inside a global complex data type (struct,union,enum)?
    char *fileName;  // The name of the file we are filtering.

    // Forward reference. Process comments when they are discovered.
    void echoComment(char *comment);

%%

  /* Complex data types */             /* Make note when we are inside one. Finishes with a global semicolon */
struct|union|enum                      ECHO; if (level == 0) complexData = true;
";"                                    ECHO; if (level == 0) complexData = false; trailing = true;

  /* Other keywords or identifiers */  /* Eat them up so they don't aren't confused with keywords */
[a-zA-Z_$]+[a-zA-Z_$0-9]*              ECHO;

  /* Quoted string literals. */        /* Pass them through. */
\"([^\\\"]|\\.)*\"                     ECHO;

  /* Character literals */            /* Pass them through. */
'(\\[tvrnafb\\]|[^\\'])'               ECHO;

   /* Single line comment. */          /* Echo a modified comment as needed. */
"//".*                                 echoComment(yytext);

   /* Multi line comment. */           /* echo a modified comment as needed. */
[/][*][^*]*[*]+([^*/][^*]*[*]+)*[/]    echoComment(yytext);

  /* Multi line #define ... */         /* Skip over #defines since they may contain unbalanced braces. */
^#define(.*\\\n)*(.*)                  ECHO;

  /* New line. */                      /* Any subsequent comment is leading, and we are no longer at start of file. */
"\n"                                   ECHO; lines++; trailing = false;

 /* Track nesting depth. */            /* Are we at the global level or not? Are we in arg list or not? */
                                       /*   We assume well formed code, so a parenthesis will never match a brace. */
"{"|"("                                ECHO; level++;
"}"|")"                                ECHO; level--; if (level == 0) complexData = false; trailing = true;

 /* Anything else */                   /* Pass it through */
.                                      ECHO;

%%
#include <stdio.h>
#include <string.h>
#include <errno.h>

// Custom banner character to be removed from comments.
//    We'll temporarily hardcode it to suit postgreSQL, but it should be set through a command line arg.
char customBanner = '-';

/***********************************************************************************************************************************
A program which reads a file, updates the comments, and writes to stdout.
This is intended to be used as a doxygen filter, converting existing comments to doxygen comments.
***********************************************************************************************************************************/
int main(int argc, char**argv) {

    // Verify we have a single argument.
    if (argc != 2) {
        fprintf(stderr, "Please run as DoxygenFilter <filename>\n");
        exit(1);
    }

    // Try to open the file.
    fileName = argv[1];
    if (freopen(fileName, "r", stdin) == NULL) {
        fprintf(stderr, "Unable to open file %s: %s\n", fileName, strerror(errno));
        exit(1);
    }

    // Now, parse the file, sending output to stdout.
    int ret = yylex();
}

// Does the string include the prefix?
bool startsWith(const char *str, const char *prefix) {
   return strncmp(str, prefix, strlen(prefix)) == 0;
}

// Output a string. The comparable stdio routine adds a newline which we don't wont.
void putstr(char *str) {
    fputs(str, stdout);
}

/***********************************************************************************************************************************
Remove a custom banner from the comment, where a banner is a sequence of two or more special characters.
***********************************************************************************************************************************/
void removeCustomBanner(char* str, char c) {
    char *writePtr = str;
    bool twoInARow = false;

    // Scan across the comment, moving non-banner characters to the front.
    for (char *readPtr=str; *readPtr != '\0'; readPtr++) {
        twoInARow = (readPtr[0] == c) && (readPtr[1] == c || twoInARow);
        if (!twoInARow)
            *writePtr++ = *readPtr;
    }
    *writePtr = '\0';
}

// Helper to output the comment prefix, either '//' or '/*'
void echoCommentBegin(char comment[]) {
    putchar(comment[0]);
    putchar(comment[1]);
}

// Helper to output the commend end, either '*/' or ''.
void echoCommentEnd(char comment[]) {
    if (comment[1] == '*')
        putstr("*/");
}

// Helper to output the commment body, everything between begin and end.
void echoCommentBody(char comment[]) {

    // Get index of where the comment end is located.
    int commentEnd = (comment[1] == '*')?  strlen(comment)-2:  strlen(comment);

    // Output body characters up to the end.
    for (int bodyIdx = 2; bodyIdx < commentEnd; bodyIdx++)
        putchar(comment[bodyIdx]);
}

/***********************************************************************************************************************************
Process and output comments as they are encountered in the text.

Uses context information gathered during scanning to decide if the comment should be converted to a doxygen comment.
***********************************************************************************************************************************/
void echoComment(char comment[]) {

    // Echo the starting part of the comment, either // or /*.
    echoCommentBegin(comment);

    // If at global level or inside a complex data declarations.
    if (level == 0 || complexData) {

        // Convert to a doxygen comment by repeating the 2nd character.
        putchar(comment[1]);  // Note: adds harmless extra char to existing doxygen comment.

        // If there is a preceding stmt on the line, then associate the doxygen comment with that stmt.
        if (trailing)
            putstr("< ");

        // Remove custom banners from doxygen comments.
        //    A custom banner is some banner other than  //////  or /*******.
        removeCustomBanner(comment, customBanner);
        }

    // Echo the comment's body.
    echoCommentBody(comment);

    // If this is a file comment, then append @file (without the optional file name) to the comment.
    if (lines == 0)
        putstr(" @file ");

    // Finally, echo the comment end sequence.
    echoCommentEnd(comment);
}
