/***********************************************************************************************************************************
Storage Test Harness

Helper functions for testing storage and related functions.
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_STORAGE_H
#define TEST_COMMON_HARNESS_STORAGE_H

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
