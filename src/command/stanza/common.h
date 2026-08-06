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
// Generate cipher info with a new pass, none when the type is none
FN_EXTERN CipherInfo *cipherInfoGen(CipherType cipherType);

// Validate and return database information
FN_EXTERN PgControl pgValidate(void);

#endif
