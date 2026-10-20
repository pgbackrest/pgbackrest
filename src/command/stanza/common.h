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
// Generate a sub pass for a file at this format, which sets the digest since that is what will derive it when it is read back
FN_EXTERN CipherSpec *cipherSpecGen(CipherType cipherType, unsigned int format);

// Warn when PBKDF2 rounds are set for a stanza at a format that does not use them
FN_EXTERN void cipherRoundCheck(unsigned int repoIdx, unsigned int format);

// PBKDF2 rounds for the info files of an encrypted repo at format >= 6. The rounds requested, else one round for a passphrase that
// looks generated, else the default.
FN_EXTERN unsigned int cipherRoundGet(unsigned int repoIdx);

// Validate and return database information
FN_EXTERN PgControl pgValidate(void);

#endif
