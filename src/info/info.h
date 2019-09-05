/***********************************************************************************************************************************
Info Handler
***********************************************************************************************************************************/
#ifndef INFO_INFO_H
#define INFO_INFO_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
#define INFO_TYPE                                                   Info
#define INFO_PREFIX                                                 info

typedef struct Info Info;
typedef struct InfoSave InfoSave;

#include "common/ini.h"
#include "storage/storage.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
#define INFO_COPY_EXT                                               ".copy"

#define INFO_KEY_FORMAT                                             "backrest-format"
    STRING_DECLARE(INFO_KEY_VERSION_STR);
#define INFO_KEY_VERSION                                            "backrest-version"
    STRING_DECLARE(INFO_KEY_FORMAT_STR);

/***********************************************************************************************************************************
Function types for loading and saving
***********************************************************************************************************************************/
typedef bool InfoLoadCallback(void *data, unsigned int try);
typedef void InfoLoadNewCallback(void *data, const String *section, const String *key, const String *value);
typedef void InfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Info *infoNew(const String *cipherPassSub);
Info *infoNewLoad(IoRead *read, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
void infoSave(Info *this, IoWrite *write, InfoSaveCallback callbackFunction, void *callbackData);
bool infoSaveSection(InfoSave *infoSaveData, const String *section, const String *sectionNext);
void infoSaveValue(InfoSave *infoSaveData, const String *section, const String *key, const String *jsonValue);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *infoCipherPass(const Info *this);

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
void infoLoad(const String *error, InfoLoadCallback callbackFunction, void *callbackData);

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
