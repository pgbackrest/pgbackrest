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
Get a file and test it against the specified content
***********************************************************************************************************************************/
typedef struct TestStorageGetParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove file after testing?
    bool nullOnMissing;                                             // NULL when file is missing
    CompressType compressType;                                      // Compression extension added to file name (limited gz and bz2)
    CipherType cipherType;
    const char *cipherPass;                                         // If pass=null but cipherType set, defaults to TEST_CIPHER_PASS
    const char *comment;                                            // Comment
} TestStorageGetParam;

#define TEST_STORAGE_GET(storage, file, expected, ...)                                                                             \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        testStorageGet(storage, file, expected, (TestStorageGetParam){VAR_PARAM_INIT, __VA_ARGS__});                               \
    }                                                                                                                              \
    while (0)

#define TEST_STORAGE_GET_EMPTY(storage, file, ...)                                                                                 \
    TEST_STORAGE_GET(storage, file, "", __VA_ARGS__)

void testStorageGet(
    const Storage *const storage, const char *const file, const char *const expected, const TestStorageGetParam param);

/***********************************************************************************************************************************
Check file exists
***********************************************************************************************************************************/
typedef struct TestStorageExistsParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove file after testing?
    const char *comment;                                            // Comment
    TimeMSec timeout;                                               // Wait for file to exist
} TestStorageExistsParam;

#define TEST_STORAGE_EXISTS(storage, file, ...)                                                                                    \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        testStorageExists(storage, file, (TestStorageExistsParam){VAR_PARAM_INIT, __VA_ARGS__});                                   \
    }                                                                                                                              \
    while (0)

void testStorageExists(const Storage *const storage, const char *const file, const TestStorageExistsParam param);

/***********************************************************************************************************************************
List files in a path and optionally remove them
***********************************************************************************************************************************/
typedef struct HrnStorageListParam
{
    VAR_PARAM_HEADER;
    bool remove;                                                    // Remove files after testing?
    bool noRecurse;                                                 // Do not recurse into subdirectories
    bool includeDot;                                                // Include . paths/links
    StorageInfoLevel level;                                         // Level of storage info to test
    bool levelForce;                                                // Use the level specified (rather than updating default)
    SortOrder sortOrder;                                            // Sort order
    const char *expression;                                         // Filter the list based on expression
    const char *comment;                                            // Comment
} HrnStorageListParam;

#define TEST_STORAGE_LIST(storage, path, expected, ...)                                                                            \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageList(storage, path, expected, (HrnStorageListParam){VAR_PARAM_INIT, __VA_ARGS__});                               \
    }                                                                                                                              \
    while (0)

#define TEST_STORAGE_LIST_EMPTY(storage, path, ...)                                                                                \
    TEST_STORAGE_LIST(storage, path, NULL, __VA_ARGS__)

void hrnStorageList(
    const Storage *const storage, const char *const path, const char *const expected, const HrnStorageListParam param);

/***********************************************************************************************************************************
Change the mode of a path/file
***********************************************************************************************************************************/
typedef struct HrnStorageModeParam
{
    VAR_PARAM_HEADER;
    mode_t mode;                                                    // Mode to set -- reset to default if not provided
    const char *comment;                                            // Comment
} HrnStorageModeParam;

#define HRN_STORAGE_MODE(storage, path, ...)                                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageMode(storage, path, (HrnStorageModeParam){VAR_PARAM_INIT, __VA_ARGS__});                                         \
    }                                                                                                                              \
    while (0)

void hrnStorageMode(const Storage *const storage, const char *const path, HrnStorageModeParam param);

/***********************************************************************************************************************************
Move a file
***********************************************************************************************************************************/
typedef struct HrnStorageMoveParam
{
    VAR_PARAM_HEADER;
    const char *comment;                                            // Comment
} HrnStorageMoveParam;

#define HRN_STORAGE_MOVE(storage, fileSource, fileDest, ...)                                                                       \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageMove(storage, fileSource, fileDest, (HrnStorageMoveParam){VAR_PARAM_INIT, __VA_ARGS__});                         \
    }                                                                                                                              \
    while (0)

void hrnStorageMove(
    const Storage *const storage, const char *const fileSource, const char *const fileDest, HrnStorageMoveParam param);

/***********************************************************************************************************************************
Copy a file
***********************************************************************************************************************************/
typedef struct HrnStorageCopyParam
{
    VAR_PARAM_HEADER;
    const char *comment;                                            // Comment
} HrnStorageCopyParam;

#define HRN_STORAGE_COPY(storageSource, fileSource, storageDest, fileDest, ...)                                                    \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageCopy(storageSource, fileSource, storageDest, fileDest, (HrnStorageCopyParam){VAR_PARAM_INIT, __VA_ARGS__});      \
    }                                                                                                                              \
    while (0)

void hrnStorageCopy(
    const Storage *const storageSource, const char *const fileSource, const Storage *const storageDest, const char *const fileDest,
    HrnStorageCopyParam param);

/***********************************************************************************************************************************
Create a path
***********************************************************************************************************************************/
typedef struct HrnStoragePathCreateParam
{
    VAR_PARAM_HEADER;
    mode_t mode;                                                    // Path mode (defaults to STORAGE_MODE_PATH_DEFAULT)
    bool noErrorOnExists;                                           // Do not error if the path already exists
    bool noParentCreate;                                            // Do not attempt to create the parent path (it must exist)
    const char *comment;                                            // Comment
} HrnStoragePathCreateParam;

#define HRN_STORAGE_PATH_CREATE(storage, path, ...)                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStoragePathCreate(storage, path, (HrnStoragePathCreateParam){VAR_PARAM_INIT, __VA_ARGS__});                             \
    }                                                                                                                              \
    while (0)

void hrnStoragePathCreate(const Storage *const storage, const char *const path, HrnStoragePathCreateParam param);

/***********************************************************************************************************************************
Remove a path
***********************************************************************************************************************************/
typedef struct HrnStoragePathRemoveParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;                                            // Error if the path is missing
    bool recurse;                                                   // Delete the path and all subpaths/files
    const char *comment;                                            // Comment
} HrnStoragePathRemoveParam;

#define HRN_STORAGE_PATH_REMOVE(storage, path, ...)                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStoragePathRemove(storage, path, (HrnStoragePathRemoveParam){VAR_PARAM_INIT, __VA_ARGS__});                             \
    }                                                                                                                              \
    while (0)

void hrnStoragePathRemove(const Storage *const storage, const char *const path, HrnStoragePathRemoveParam param);

/***********************************************************************************************************************************
Put a file with optional compression and/or encryption
***********************************************************************************************************************************/
typedef struct HrnStoragePutParam
{
    VAR_PARAM_HEADER;
    mode_t modeFile;                                                // File mode if not the default
    time_t timeModified;                                            // Time file was last modified
    CompressType compressType;                                      // Limited to gz and bz2 (lz4 and zstd are not always available)
    CipherType cipherType;
    const char *cipherPass;
    const char *comment;                                            // Comment
} HrnStoragePutParam;

#define HRN_STORAGE_PUT(storage, file, buffer, ...)                                                                                \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStoragePut(storage, file, buffer, NULL, (HrnStoragePutParam){VAR_PARAM_INIT, __VA_ARGS__});                             \
    }                                                                                                                              \
    while (0)

#define HRN_STORAGE_PUT_EMPTY(storage, file, ...)                                                                                  \
    HRN_STORAGE_PUT(storage, file, NULL, __VA_ARGS__)

#define HRN_STORAGE_PUT_Z(storage, file, stringz, ...)                                                                             \
    HRN_STORAGE_PUT(storage, file, BUFSTRZ(stringz), __VA_ARGS__)

void hrnStoragePut(
    const Storage *const storage, const char *const file, const Buffer *const buffer, const char *const logPrefix,
    HrnStoragePutParam param);

/***********************************************************************************************************************************
Remove a file
***********************************************************************************************************************************/
typedef struct HrnStorageRemoveParam
{
    VAR_PARAM_HEADER;
    bool errorOnMissing;                                            // Error when the file is missing
    const char *comment;                                            // Comment
} HrnStorageRemoveParam;

#define HRN_STORAGE_REMOVE(storage, file, ...)                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageRemove(storage, file, (HrnStorageRemoveParam){VAR_PARAM_INIT, __VA_ARGS__});                                     \
    }                                                                                                                              \
    while (0)

void hrnStorageRemove(const Storage *const storage, const char *const file, const HrnStorageRemoveParam param);

/***********************************************************************************************************************************
Change the time of a path/file
***********************************************************************************************************************************/
typedef struct HrnStorageTimeParam
{
    VAR_PARAM_HEADER;
    const char *comment;                                            // Comment
} HrnStorageTimeParam;

#define HRN_STORAGE_TIME(storage, path, time, ...)                                                                                 \
    do                                                                                                                             \
    {                                                                                                                              \
        hrnTestLogPrefix(__LINE__);                                                                                                \
        hrnStorageTime(storage, path, time, (HrnStorageTimeParam){VAR_PARAM_INIT, __VA_ARGS__});                                   \
    }                                                                                                                              \
    while (0)

void hrnStorageTime(const Storage *const storage, const char *const path, const time_t modified, const HrnStorageTimeParam param);

/***********************************************************************************************************************************
Dummy interface for constructing test storage drivers. All functions and features are pulled from the Posix driver.
***********************************************************************************************************************************/
extern const StorageInterface hrnStorageInterfaceDummy;

#endif
