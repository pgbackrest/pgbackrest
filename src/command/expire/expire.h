/***********************************************************************************************************************************
Expire Command
***********************************************************************************************************************************/
#ifndef COMMAND_EXPIRE_EXPIRE_H
#define COMMAND_EXPIRE_EXPIRE_H

#include <stdbool.h>

// CSHANG How to do this?
// /***********************************************************************************************************************************
// backupRegExpGet
// ***********************************************************************************************************************************/
// typedef struct BackupRegExpGetParam
// {
//     bool full;
//     bool differential;
//     bool incremental;
//     bool anchor;
// } BackupRegExpGetParam;
//
// #define backupRegExpGetP(this, pathExp, ...)
//     backupRegExpGet((BackupRegExpGetParam){__VA_ARGS__})
// #define backupRegExpGetNP(this, pathExp)
//     backupRegExpGet((BackupRegExpGetParam){0})
//
// String *backupRegExpGet(BackupRegExpGetParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void cmdExpire(void);

#endif
