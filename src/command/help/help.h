/***********************************************************************************************************************************
Help Command
***********************************************************************************************************************************/
#ifndef COMMAND_HELP_HELP_H
#define COMMAND_HELP_HELP_H

#include "common/type/string.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Render help and output to stdout
FN_EXTERN void cmdHelp(void);

// Render help text
FN_EXTERN String *helpRenderText(const String *text, bool internal, bool beta, size_t indent, bool indentFirst, size_t length);

#endif
