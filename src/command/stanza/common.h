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
String *cipherPassGen(CipherType cipherType);
void infoValidate(const InfoPgData *archiveInfo, const InfoPgData *backupInfo);
PgControl pgValidate(void);

#endif
