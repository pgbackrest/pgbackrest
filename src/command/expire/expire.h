/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#ifndef COMMAND_EXPIRE_EXPIRE_H
#define COMMAND_EXPIRE_EXPIRE_H

#include <stdbool.h>

#include "common/type/string.h"

/***********************************************************************************************************************************
backupRegExpGet
***********************************************************************************************************************************/
typedef struct BackupRegExpGetParam
{
    bool full;
    bool differential;
    bool incremental;
    bool noAnchor;
} BackupRegExpGetParam;

#define backupRegExpGetP(...)                                                                                                      \
    backupRegExpGet((BackupRegExpGetParam){__VA_ARGS__})
#define backupRegExpGetNP()                                                                                                        \
    backupRegExpGet((BackupRegExpGetParam){0})

String *backupRegExpGet(BackupRegExpGetParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cmdExpire(void);

#endif
