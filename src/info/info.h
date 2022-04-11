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

#include "common/ini.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_COPY_EXT                                               ".copy"

#define INFO_KEY_FORMAT                                             "backrest-format"
    STRING_DECLARE(INFO_KEY_FORMAT_STR);
#define INFO_KEY_VERSION                                            "backrest-version"
    STRING_DECLARE(INFO_KEY_VERSION_STR);

/***********************************************************************************************************************************
Function types for loading and saving
***********************************************************************************************************************************/
// The purpose of this callback is to attempt a load (from file or otherwise).  Return true when the load is successful or throw an
// error.  Return false when there are no more loads to try, but always make at least one load attempt.  The try parameter will
// start at 0 and be incremented on each call.
typedef bool InfoLoadCallback(void *data, unsigned int try);

typedef void InfoLoadNewCallback(void *data, const String *section, const String *key, const Variant *value);
typedef void InfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Info *infoNew(const String *cipherPassSub);

// Create new object and load contents from a file
Info *infoNewLoad(IoRead *read, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct InfoPub
{
    const String *backrestVersion;                                  // pgBackRest version
    const String *cipherPass;                                       // Cipher passphrase if set
} InfoPub;

// Cipher passphrase if set
__attribute__((always_inline)) static inline const String *
infoCipherPass(const Info *const this)
{
    return THIS_PUB(Info)->cipherPass;
}

void infoCipherPassSet(Info *this, const String *cipherPass);

// pgBackRest version
__attribute__((always_inline)) static inline const String *
infoBackrestVersion(const Info *const this)
{
    return THIS_PUB(Info)->backrestVersion;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Save to file
void infoSave(Info *this, IoWrite *write, InfoSaveCallback *callbackFunction, void *callbackData);

// Check if the section should be saved
bool infoSaveSection(InfoSave *infoSaveData, const String *section, const String *sectionNext);

// Save a JSON formatted value and update checksum
void infoSaveValue(InfoSave *infoSaveData, const String *section, const String *key, const String *jsonValue);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Load info file(s) and throw error for each attempt if none are successful
void infoLoad(const String *error, InfoLoadCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_INFO_TYPE                                                                                                     \
    Info *
#define FUNCTION_LOG_INFO_FORMAT(value, buffer, bufferSize)                                                                        \
    objToLog(value, "Info", buffer, bufferSize)

#define FUNCTION_LOG_INFO_SAVE_TYPE                                                                                                \
    InfoSave *
#define FUNCTION_LOG_INFO_SAVE_FORMAT(value, buffer, bufferSize)                                                                   \
    objToLog(value, "InfoSave", buffer, bufferSize)

#endif
