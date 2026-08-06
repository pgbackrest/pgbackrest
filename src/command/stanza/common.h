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
// Generate a sub pass for a file at this format, which sets the digest since that is what will derive it when it is read back
FN_EXTERN CipherSpec *cipherSpecGen(CipherType cipherType, unsigned int format);

// Validate and return database information
FN_EXTERN PgControl pgValidate(void);

#endif
