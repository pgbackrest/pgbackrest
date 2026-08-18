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
// Generate cipher spec with a new pass, none when the type is none
FN_EXTERN CipherSpec *cipherSpecGen(CipherType cipherType);

// Validate and return database information
FN_EXTERN PgControl pgValidate(void);

#endif
