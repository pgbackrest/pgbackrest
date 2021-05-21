/***********************************************************************************************************************************
Storage Test Harness

Helper functions for testing storage and related functions.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_H
#define TEST_COMMON_HARNESS_STORAGE_H

#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Check file exists
***********************************************************************************************************************************/
#define TEST_STORAGE_EXISTS(storage, file)                                                                                         \
    TEST_RESULT_BOOL(storageExistsP(storage, STR(file)), true, "file exists '%s'", strZ(storagePathP(storage, STR(file))))

/***********************************************************************************************************************************
Get a file and test it against the specified content
***********************************************************************************************************************************/
typedef struct TestStorageGetParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove file after testing?
} TestStorageGetParam;

#define TEST_STORAGE_GET(storage, file, content, ...)                                                                              \
    testStorageGet(__LINE__, storage, file, content, (TestStorageGetParam){VAR_PARAM_INIT, __VA_ARGS__})
#define TEST_STORAGE_GET_EMPTY(storage, file, ...)                                                                                 \
    TEST_STORAGE_GET(storage, file, "", __VA_ARGS__)

void testStorageGet(
    const int line, const Storage *const storage, const char *const file, const char *const expected, TestStorageGetParam param);

/***********************************************************************************************************************************
List files in a path and optionally remove them
***********************************************************************************************************************************/
typedef struct HrnStorageListParam
{
    VAR_PARAM_HEADER;
    bool remove;
} HrnStorageListParam;

#define TEST_STORAGE_LIST(storage, path, expected, ...)                                                                            \
    hrnStorageList(__LINE__, storage, path, expected, (HrnStorageListParam){VAR_PARAM_INIT, __VA_ARGS__})

#define TEST_STORAGE_LIST_EMPTY(storage, path, ...)                                                                                \
    TEST_STORAGE_LIST(storage, path, NULL, __VA_ARGS__)

void hrnStorageList(
    const int line, const Storage *const storage, const char *const path, const char *const expected,
    const HrnStorageListParam param);

/***********************************************************************************************************************************
Change the mode of a path/file
***********************************************************************************************************************************/
typedef struct HrnStorageModeParam
{
    VAR_PARAM_HEADER;
    mode_t mode;                                                    // Mode to set -- reset to default if not provided
} HrnStorageModeParam;

#define HRN_STORAGE_MODE(storage, path, ...)                                                                                       \
    hrnStorageMode(__LINE__, storage, path, (HrnStorageModeParam){VAR_PARAM_INIT, __VA_ARGS__})

void hrnStorageMode(const int line, const Storage *const storage, const char *const path, HrnStorageModeParam param);

/***********************************************************************************************************************************
Put a file with optional compression and/or encryption
***********************************************************************************************************************************/
typedef struct HrnStoragePutParam
{
    VAR_PARAM_HEADER;
    CompressType compressType;
    CipherType cipherType;
    const char *cipherPass;
} HrnStoragePutParam;

#define HRN_STORAGE_PUT(storage, file, buffer, ...)                                                                                \
    TEST_RESULT_VOID(                                                                                                              \
        hrnStoragePut(storage, file, buffer, (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__}), "put file %s",                    \
        hrnStoragePutLog(storage, file, buffer, (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__}))

#define HRN_STORAGE_PUT_EMPTY(storage, file, ...)                                                                                  \
    HRN_STORAGE_PUT(storage, file, NULL, __VA_ARGS__)

#define HRN_STORAGE_PUT_Z(storage, file, stringz, ...)                                                                             \
    HRN_STORAGE_PUT(storage, file, BUFSTRZ(stringz), __VA_ARGS__)

void hrnStoragePut(const Storage *storage, const char *file, const Buffer *buffer, HrnStoragePutParam param);
const char *hrnStoragePutLog(const Storage *storage, const char *file, const Buffer *buffer, HrnStoragePutParam param);

/***********************************************************************************************************************************
Remove a file and error if it does not exist
***********************************************************************************************************************************/
#define TEST_STORAGE_REMOVE(storage, path)                                                                                         \
    TEST_RESULT_VOID(storageRemoveP(storage, STR(path), .errorOnMissing = true), "remove file '%s'",                               \
    strZ(storagePathP(storage, STR(path))))

/***********************************************************************************************************************************
Change the time of a path/file
***********************************************************************************************************************************/
#define HRN_STORAGE_TIME(storage, path, time)                                                                                      \
    hrnStorageTime(__LINE__, storage, path, time)

void hrnStorageTime(const int line, const Storage *const storage, const char *const path, const time_t modified);

/***********************************************************************************************************************************
Dummy interface for constructing test storage drivers. All required functions are stubbed out so this interface can be copied and
specific functions replaced for testing.
***********************************************************************************************************************************/
extern const StorageInterface storageInterfaceTestDummy;

/***********************************************************************************************************************************
Callback for formatting info list results
***********************************************************************************************************************************/
typedef struct HarnessStorageInfoListCallbackData
{
    const Storage *storage;                                         // Storage object when needed (e.g. fileCompressed = true)
    const String *path;                                             // subpath when storage is specified

    String *content;                                                // String where content should be added
    bool modeOmit;                                                  // Should the specified mode be omitted?
    mode_t modeFile;                                                // File to omit if modeOmit is true
    mode_t modePath;                                                // Path mode to omit if modeOmit is true
    bool timestampOmit;                                             // Should the timestamp be omitted?
    bool userOmit;                                                  // Should the current user be omitted?
    bool groupOmit;                                                 // Should the current group be omitted?
    bool sizeOmit;                                                  // Should the size be omitted
    bool rootPathOmit;                                              // Should the root path be omitted?
    bool fileCompressed;                                            // Files will be decompressed to get size
} HarnessStorageInfoListCallbackData;

void hrnStorageInfoListCallback(void *callbackData, const StorageInfo *info);

#endif
