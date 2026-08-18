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

#include "common/crypto/spec.h"
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
FN_EXTERN Info *infoNew(unsigned int format, const CipherSpec *cipherSpecSub);

// Create new object and load contents from a file. The cipher spec the file is read with supplies the type for the cipher spec
// built from the pass stored in it.
FN_EXTERN Info *infoNewLoad(
    IoRead *read, const CipherSpec *cipherSpec, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoPub
{
    unsigned int format;                                            // Repository format the file was written with
    const String *backrestVersion;                                  // pgBackRest version
    const CipherSpec *cipherSpec;                                   // Cipher spec for dependent files
} InfoPub;

// Repository format
FN_INLINE_ALWAYS unsigned int
infoFormat(const Info *const this)
{
    return THIS_PUB(Info)->format;
}

// Set the repository format, which is how a stanza is migrated to a newer format
FN_EXTERN void infoFormatSet(Info *this, unsigned int format);

// Cipher spec for the files that depend on this one, e.g. the manifest for backup.info. Never NULL, so it can be handed on
// without a check, and none when there is no pass.
FN_INLINE_ALWAYS const CipherSpec *
infoCipherSpec(const Info *const this)
{
    return THIS_PUB(Info)->cipherSpec;
}

// Set cipher spec for dependent files. NULL means they are not encrypted.
FN_EXTERN void infoCipherSpecSet(Info *this, const CipherSpec *cipherSpec);

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
