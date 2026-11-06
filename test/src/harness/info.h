/***********************************************************************************************************************************
Harness for Generating Test Info Files
***********************************************************************************************************************************/
#include "common/format/format.h"
#include "common/type/buffer.h"
#include "info/info.h"

#include "harness/storage.h"

/***********************************************************************************************************************************
Format that new repositories are created with, i.e. the default of the repo-format option in build/config.yaml. Only tests need to
know this since the option supplies it everywhere else.
***********************************************************************************************************************************/
#define REPOSITORY_FORMAT_DEFAULT                                   REPOSITORY_FORMAT_5

/***********************************************************************************************************************************
Write info to a file and add the checksum, storing it the way the format stores it when it is encrypted
***********************************************************************************************************************************/
typedef struct HrnInfoPutParam
{
    VAR_PARAM_HEADER;
    unsigned int format;                                            // Repository format, default format when zero
    bool header;                                                    // Does the file contain a header, i.e. is it an info file?
    const CipherSpec *cipherSpec;                                   // Cipher spec when the file is encrypted, digest set by format
    const char *comment;                                            // Comment
} HrnInfoPutParam;

#define HRN_INFO_PUT(storage, file, info, ...)                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnInfoPut(storage, file, info, (HrnInfoPutParam){VAR_PARAM_INIT, __VA_ARGS__});                                           \
    }                                                                                                                              \
    while (0)

void hrnInfoPut(const Storage *storage, const char *file, const char *info, HrnInfoPutParam param);

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
Buffer *harnessInfoChecksum(const String *info);
Buffer *harnessInfoChecksumFormat(unsigned int format, const String *info);
Buffer *harnessInfoChecksumZ(const char *info);

// Encrypt content the way a file at a format is stored, i.e. with the header and digest that go with the format. No format means
// the file has none of its own, e.g. a manifest, so it is stored with no header and the digest the spec was given.
typedef struct HarnessInfoEncryptParam
{
    VAR_PARAM_HEADER;
    unsigned int format;                                            // Repository format the file is stored at
} HarnessInfoEncryptParam;

#define harnessInfoEncryptP(content, cipherSpec, ...)                                                                              \
    harnessInfoEncrypt(content, cipherSpec, (HarnessInfoEncryptParam){VAR_PARAM_INIT, __VA_ARGS__})

Buffer *harnessInfoEncrypt(const Buffer *content, const CipherSpec *cipherSpec, HarnessInfoEncryptParam param);

void harnessInfoLoadNewCallback(void *callbackData, const String *section, const String *key, JsonRead *json);
