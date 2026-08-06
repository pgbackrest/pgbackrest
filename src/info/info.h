/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFO_H
#define INFO_INFO_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct Info Info;
typedef struct InfoSave InfoSave;

#include "common/crypto/info.h"
#include "common/ini.h"
#include "common/type/json.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_COPY_EXT                                               ".copy"

#define INFO_KEY_FORMAT                                             "backrest-format"
#define INFO_KEY_VERSION                                            "backrest-version"

/***********************************************************************************************************************************
Function types for loading and saving
***********************************************************************************************************************************/
// The purpose of this callback is to attempt a load (from file or otherwise). Return true when the load is successful or throw an
// error. Return false when there are no more loads to try, but always make at least one load attempt. The try parameter will start
// at 0 and be incremented on each call.
// {uncrustify_off - uncrustify unable to parse this statement}
typedef bool InfoLoadCallback(void *data, unsigned int try);
// {uncrustify_on}

typedef void InfoLoadNewCallback(void *data, const String *section, const String *key, JsonRead *value);
typedef void InfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN Info *infoNew(const CipherInfo *cipherInfoSub);

// Create new object and load contents from a file. The cipher info the file is read with supplies the type for the cipher info
// built from the pass stored in it.
FN_EXTERN Info *infoNewLoad(
    IoRead *read, const CipherInfo *cipherInfo, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoPub
{
    const String *backrestVersion;                                  // pgBackRest version
    const CipherInfo *cipherInfo;                                   // Cipher info for dependent files
} InfoPub;

// Cipher info for the files that depend on this one, e.g. the manifest for backup.info. Never NULL, so it can be handed on
// without a check, and none when there is no pass.
FN_INLINE_ALWAYS const CipherInfo *
infoCipherInfo(const Info *const this)
{
    return THIS_PUB(Info)->cipherInfo;
}

// Set cipher info for dependent files. NULL means they are not encrypted.
FN_EXTERN void infoCipherInfoSet(Info *this, const CipherInfo *cipherInfo);

// pgBackRest version
FN_INLINE_ALWAYS const String *
infoBackrestVersion(const Info *const this)
{
    return THIS_PUB(Info)->backrestVersion;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Save to file
FN_EXTERN void infoSave(Info *this, IoWrite *write, InfoSaveCallback *callbackFunction, void *callbackData);

// Check if the section should be saved
FN_EXTERN bool infoSaveSection(InfoSave *infoSaveData, const char *section, const String *sectionNext);

// Save a JSON formatted value and update checksum
FN_EXTERN void infoSaveValue(InfoSave *infoSaveData, const char *section, const char *key, const String *jsonValue);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load info file(s) and throw error for each attempt if none are successful
FN_EXTERN void infoLoad(const String *error, InfoLoadCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_TYPE                                                                                                     \
    Info *
#define FUNCTION_LOG_INFO_FORMAT(value, buffer, bufferSize)                                                                        \
    objNameToLog(value, "Info", buffer, bufferSize)

#define FUNCTION_LOG_INFO_SAVE_TYPE                                                                                                \
    InfoSave *
#define FUNCTION_LOG_INFO_SAVE_FORMAT(value, buffer, bufferSize)                                                                   \
    objNameToLog(value, "InfoSave", buffer, bufferSize)

#endif
