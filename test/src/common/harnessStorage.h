/***********************************************************************************************************************************
Storage Test Harness

Helper functions for testing storage and related functions.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_H
#define TEST_COMMON_HARNESS_STORAGE_H

#include "storage/storage.intern.h"

/***********************************************************************************************************************************
List files in a path and join the files into a string separated by |
***********************************************************************************************************************************/
const char *hrnStorageListJoin(const Storage *storage, const char *path);

#define TEST_STORAGE_LIST_Z(storage, path, list)                                                                                   \
    TEST_RESULT_Z(hrnStorageListJoin(storage, path), list, "check " #storage "/" #path " files")

#define TEST_STORAGE_LIST_STR_Z(storage, path, list)                                                                               \
    TEST_STORAGE_LIST_Z(storage, strZ(path), list)

/***********************************************************************************************************************************
List files in a path and join the files into a string separated by | and then remove them
***********************************************************************************************************************************/
const char *hrnStorageListJoinRemove(const Storage *storage, const char *path);

#define TEST_STORAGE_LIST_REMOVE_Z(storage, path, list)                                                                            \
    TEST_RESULT_Z(hrnStorageListJoinRemove(storage, path), list, "check and remove " #storage "/" #path " files")

#define TEST_STORAGE_LIST_REMOVE_STR_Z(storage, path, list)                                                                        \
    TEST_STORAGE_LIST_REMOVE_Z(storage, strZ(path), list)

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
