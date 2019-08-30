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
Function types for loading and saving
***********************************************************************************************************************************/
typedef void InfoLoadNewCallback(InfoCallbackType type, void *data, const String *section, const String *key, const String *value);
typedef void InfoSaveCallback(void *data, const String *sectionNext, InfoSave *infoSaveData);

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
Info *infoNew(CipherType cipherType, const String *cipherPassSub);
Info *infoNewLoad(IoRead *read, InfoLoadNewCallback *callbackFunction, void *callbackData);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Info *infoMove(Info *this, MemContext *parentNew);
void infoSave(Info *this, IoWrite *write, InfoSaveCallback callbackFunction, void *callbackData);
bool infoSaveSection(InfoSave *infoSaveData, const String *section, const String *sectionNext);
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
