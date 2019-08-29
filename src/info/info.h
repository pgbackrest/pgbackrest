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

#include "common/crypto/common.h"
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
Info callback types used during ini processing
***********************************************************************************************************************************/
typedef enum
{
    infoCallbackTypeBegin,                                          // Load of the ini is beginning
    infoCallbackTypeReset,                                          // An error occurred and data should be reset
    infoCallbackTypeValue,                                          // A section/key/value from the ini
    infoCallbackTypeEnd,                                            // Load of the ini has ended
} InfoCallbackType;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Info *infoNew(CipherType cipherType, const String *cipherPassSub);
Info *infoNewLoad(
    const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass,
    void (*callbackFunction)(InfoCallbackType type, void *data, const String *section, const String *key, const String *value),
    void *callbackData);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
#define INFO_SAVE_SECTION(section)                                                                                                 \
    ((sectionLast == NULL || strCmp(section, sectionLast) > 0) && (sectionNext == NULL || strCmp(section, sectionNext) < 0))

typedef void InfoSaveCallback(void *data, const String *sectionLast, const String *sectionNext, InfoSave *infoSaveData);

void infoSave(
    Info *this, const Storage *storage, const String *fileName, CipherType cipherType, const String *cipherPass,
    InfoSaveCallback *callbackFunction, void *callbackData);
void infoSaveValue(InfoSave *infoSaveData, const String *section, const String *key, const String *value);

/***********************************************************************************************************************************
Getters
***********************************************************************************************************************************/
const String *infoCipherPass(const Info *this);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
void infoFree(Info *this);

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
