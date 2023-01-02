/***********************************************************************************************************************************
Stanza Commands Handler
***********************************************************************************************************************************/
#ifndef COMMAND_STANZA_COMMON_H
#define COMMAND_STANZA_COMMON_H

#include "info/infoPg.h"
#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Generate a cipher
FN_EXTERN String *cipherPassGen(CipherType cipherType);

// Validate and return database information
FN_EXTERN PgControl pgValidate(void);

#endif
