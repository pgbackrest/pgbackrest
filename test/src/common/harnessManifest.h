/***********************************************************************************************************************************
Harness for Manifest Testing
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_MANIFEST_H
#define TEST_COMMON_HARNESS_MANIFEST_H

#include "common/user.h"
#include "info/manifest.h"

/***********************************************************************************************************************************
Add db to manifest
***********************************************************************************************************************************/
typedef struct HrnManifestDb
{
    const char *name;                                               // See ManifestDb for comments
    unsigned int id;
    unsigned int lastSystemId;
} HrnManifestDb;

#define HRN_MANIFEST_DB_ADD(manifest, ...)                                                                                       \
    hrnManifestDbAdd(manifest, (HrnManifestDb){__VA_ARGS__});

void hrnManifestDbAdd(Manifest *manifest, HrnManifestDb hrnManifestDb);

/***********************************************************************************************************************************
Add file to manifest
***********************************************************************************************************************************/
typedef struct HrnManifestFile
{
    const char *name;                                               // See ManifestFile for comments
    bool copy : 1;
    bool delta : 1;
    bool resume : 1;
    bool checksumPage : 1;
    bool checksumPageError : 1;
    mode_t mode;
    const char *checksumSha1;
    const char *checksumRepoSha1;
    const String *checksumPageErrorList;
    const char *user;
    const char *group;
    const char *reference;
    uint64_t bundleId;
    uint64_t bundleOffset;
    size_t blockIncrSize;
    size_t blockIncrChecksumSize;
    uint64_t blockIncrMapSize;
    uint64_t size;
    uint64_t sizeOriginal;
    uint64_t sizeRepo;
    time_t timestamp;
} HrnManifestFile;

#define HRN_MANIFEST_FILE_ADD(manifest, ...)                                                                                       \
    hrnManifestFileAdd(manifest, (HrnManifestFile){__VA_ARGS__});

void hrnManifestFileAdd(Manifest *manifest, HrnManifestFile hrnManifestFile);

/***********************************************************************************************************************************
Add link to manifest
***********************************************************************************************************************************/
typedef struct HrnManifestLink
{
    const char *name;                                               // See ManifestLink for comments
    const char *destination;
    const char *user;
    const char *group;
} HrnManifestLink;

#define HRN_MANIFEST_LINK_ADD(manifest, ...)                                                                                       \
    hrnManifestLinkAdd(manifest, (HrnManifestLink){__VA_ARGS__});

void hrnManifestLinkAdd(Manifest *manifest, HrnManifestLink hrnManifestLink);

/***********************************************************************************************************************************
Add path to manifest
***********************************************************************************************************************************/
typedef struct HrnManifestPath
{
    const char *name;                                               // See ManifestPath for comments
    mode_t mode;
    const char *user;
    const char *group;
} HrnManifestPath;

#define HRN_MANIFEST_PATH_ADD(manifest, ...)                                                                                       \
    hrnManifestPathAdd(manifest, (HrnManifestPath){__VA_ARGS__});

void hrnManifestPathAdd(Manifest *manifest, HrnManifestPath hrnManifestPath);

/***********************************************************************************************************************************
Add target to manifest
***********************************************************************************************************************************/
typedef struct HrnManifestTarget
{
    const char *name;                                               // See ManifestTarget for comments
    ManifestTargetType type;
    const char *path;
    const char *file;
    unsigned int tablespaceId;
    const char *tablespaceName;
} HrnManifestTarget;

#define HRN_MANIFEST_TARGET_ADD(manifest, ...)                                                                                       \
    hrnManifestTargetAdd(manifest, (HrnManifestTarget){__VA_ARGS__});

void hrnManifestTargetAdd(Manifest *manifest, HrnManifestTarget hrnManifestTarget);

#endif
