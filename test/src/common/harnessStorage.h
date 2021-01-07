/***********************************************************************************************************************************
Storage Test Harness

Helper functions for testing storage and related functions.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_H
#define TEST_COMMON_HARNESS_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
Check file exists
***********************************************************************************************************************************/
#define TEST_FILE_EXISTS_STR(storage, path)                                                                                        \
    TEST_RESULT_BOOL(storageExistsP(storage, path), true, "file exists '%s'", strZ(storagePathP(storage, path)))

#define TEST_FILE_EXISTS_Z(storage, path)                                                                                          \
    TEST_FILE_EXISTS_STR(storage, STR(path))

/***********************************************************************************************************************************
List files in a path and join the files into a string separated by |
***********************************************************************************************************************************/
const char *hrnStorageList(const Storage *storage, const char *path);

#define TEST_FILE_LIST_Z(storage, path, list)                                                                                      \
    TEST_RESULT_Z(hrnStorageList(storage, path), list, "check files in '%s'", strZ(storagePathP(storage, STR(path))))

#define TEST_FILE_LIST_STR_Z(storage, path, list)                                                                                  \
    TEST_FILE_LIST_Z(storage, strZ(path), list)

/***********************************************************************************************************************************
List files in a path and join the files into a string separated by | and then remove them
***********************************************************************************************************************************/
const char *hrnStorageListRemove(const Storage *storage, const char *path);

#define TEST_FILE_LIST_REMOVE_Z(storage, path, list)                                                                               \
    TEST_RESULT_Z(hrnStorageListRemove(storage, path), list, "check/remove files in '%s'", strZ(storagePathP(storage, STR(path))))

#define TEST_FILE_LIST_REMOVE_STR_Z(storage, path, list)                                                                           \
    TEST_FILE_LIST_REMOVE_Z(storage, strZ(path), list)

/***********************************************************************************************************************************
Write a file
***********************************************************************************************************************************/
#define TEST_FILE_PUT_Z_BUF(storage, path, buffer)                                                                                 \
    TEST_FILE_PUT_STR_BUF(storage, STR(path), buffer)

#define TEST_FILE_PUT_STR_BUF(storage, path, buffer)                                                                               \
    TEST_RESULT_VOID(                                                                                                              \
        storagePutP(storageNewWriteP(storage, path), buffer), "put %zub file '%s'", bufSize(buffer),                               \
        strZ(storagePathP(storage, path)))

#define TEST_FILE_PUT_EMPTY_Z(storage, path)                                                                                       \
    TEST_FILE_PUT_EMPTY_STR(storage, STR(path));

#define TEST_FILE_PUT_EMPTY_STR(storage, path)                                                                                     \
    TEST_RESULT_VOID(storagePutP(storageNewWriteP(storage, path), NULL), "put empty file '%s'", strZ(storagePathP(storage, path)))

/***********************************************************************************************************************************
Remove a file and error if it does not exist
***********************************************************************************************************************************/
#define TEST_FILE_REMOVE_Z(storage, path)                                                                                          \
    TEST_FILE_REMOVE_STR(storage, STR(path))

#define TEST_FILE_REMOVE_STR(storage, path)                                                                                        \
    TEST_RESULT_VOID(storageRemoveP(storage, path, .errorOnMissing = true), "remove file '%s'", strZ(storagePathP(storage, path)))

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
