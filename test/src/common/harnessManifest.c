/***********************************************************************************************************************************
Harness for Manifest Testing
***********************************************************************************************************************************/
#include "build.auto.h"

#include "common/harnessDebug.h"
#include "common/harnessManifest.h"

/***********************************************************************************************************************************
Include shimmed C modules
***********************************************************************************************************************************/
{[SHIM_MODULE]}

/**********************************************************************************************************************************/
void
hrnManifestDbAdd(Manifest *const manifest, const HrnManifestDb hrnManifestDb)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestDb.name);
        FUNCTION_HARNESS_PARAM(UINT, hrnManifestDb.id);
        FUNCTION_HARNESS_PARAM(UINT, hrnManifestDb.lastSystemId);
    FUNCTION_HARNESS_END();

    ASSERT(hrnManifestDb.name != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ManifestDb manifestDb =
        {
            .name = STR(hrnManifestDb.name),
            .id = hrnManifestDb.id,
            .lastSystemId = hrnManifestDb.lastSystemId,
        };

        manifestDbAdd(manifest, &manifestDb);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnManifestFileAdd(Manifest *const manifest, const HrnManifestFile hrnManifestFile)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.name);
        FUNCTION_HARNESS_PARAM(BOOL, hrnManifestFile.copy);
        FUNCTION_HARNESS_PARAM(BOOL, hrnManifestFile.delta);
        FUNCTION_HARNESS_PARAM(BOOL, hrnManifestFile.resume);
        FUNCTION_HARNESS_PARAM(BOOL, hrnManifestFile.checksumPage);
        FUNCTION_HARNESS_PARAM(BOOL, hrnManifestFile.checksumPageError);
        FUNCTION_HARNESS_PARAM(MODE, hrnManifestFile.mode);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.checksumSha1);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.checksumRepoSha1);
        FUNCTION_HARNESS_PARAM(STRING, hrnManifestFile.checksumPageErrorList);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.user);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.group);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestFile.reference);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.bundleId);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.bundleOffset);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.blockIncrSize);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.blockIncrMapSize);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.size);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.sizeOriginal);
        FUNCTION_HARNESS_PARAM(UINT64, hrnManifestFile.sizeRepo);
        FUNCTION_HARNESS_PARAM(TIME, hrnManifestFile.timestamp);
    FUNCTION_HARNESS_END();

    ASSERT(hrnManifestFile.name != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ManifestFile manifestFile =
        {
            .name = STR(hrnManifestFile.name),
            .copy = hrnManifestFile.copy,
            .delta = hrnManifestFile.delta,
            .resume = hrnManifestFile.resume,
            .checksumPage = hrnManifestFile.checksumPage,
            .checksumPageError = hrnManifestFile.checksumPageError,
            .checksumPageErrorList = hrnManifestFile.checksumPageErrorList,
            .bundleId = hrnManifestFile.bundleId,
            .bundleOffset = hrnManifestFile.bundleOffset,
            .blockIncrSize = hrnManifestFile.blockIncrSize,
            .blockIncrChecksumSize = hrnManifestFile.blockIncrChecksumSize,
            .blockIncrMapSize = hrnManifestFile.blockIncrMapSize,
            .size = hrnManifestFile.size,
            .sizeOriginal = hrnManifestFile.sizeOriginal,
            .sizeRepo = hrnManifestFile.sizeRepo,
            .timestamp = hrnManifestFile.timestamp,
        };

        if (manifestFile.sizeOriginal == 0)
            manifestFile.sizeOriginal = manifestFile.size;

        if (hrnManifestFile.mode == 0)
            manifestFile.mode = 0600;
        else
            manifestFile.mode = hrnManifestFile.mode;

        if (hrnManifestFile.user != NULL)
            manifestFile.user = strNewZ(hrnManifestFile.user);
        else
            manifestFile.user = userName();

        if (hrnManifestFile.group != NULL)
            manifestFile.group = strNewZ(hrnManifestFile.group);
        else
            manifestFile.group = groupName();

        if (hrnManifestFile.reference != NULL)
            manifestFile.reference = strNewZ(hrnManifestFile.reference);

        if (hrnManifestFile.checksumSha1 != NULL)
            manifestFile.checksumSha1 = bufPtr(bufNewDecode(encodingHex, STR(hrnManifestFile.checksumSha1)));

        if (hrnManifestFile.checksumRepoSha1 != NULL)
            manifestFile.checksumRepoSha1 = bufPtr(bufNewDecode(encodingHex, STR(hrnManifestFile.checksumRepoSha1)));

        manifestFileAdd(manifest, &manifestFile);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnManifestLinkAdd(Manifest *const manifest, const HrnManifestLink hrnManifestLink)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestLink.name);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestLink.destination);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestLink.user);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestLink.group);
    FUNCTION_HARNESS_END();

    ASSERT(hrnManifestLink.name != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ManifestLink manifestLink = {.name = STR(hrnManifestLink.name)};

        if (hrnManifestLink.destination != NULL)
            manifestLink.destination = strNewZ(hrnManifestLink.destination);

        if (hrnManifestLink.user != NULL)
            manifestLink.user = strNewZ(hrnManifestLink.user);
        else
            manifestLink.user = userName();

        if (hrnManifestLink.group != NULL)
            manifestLink.group = strNewZ(hrnManifestLink.group);
        else
            manifestLink.group = groupName();

        manifestLinkAdd(manifest, &manifestLink);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnManifestPathAdd(Manifest *const manifest, const HrnManifestPath hrnManifestPath)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestPath.name);
        FUNCTION_HARNESS_PARAM(MODE, hrnManifestPath.mode);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestPath.user);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestPath.group);
    FUNCTION_HARNESS_END();

    ASSERT(hrnManifestPath.name != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ManifestPath manifestPath = {.name = STR(hrnManifestPath.name)};

        if (hrnManifestPath.mode == 0)
            manifestPath.mode = 0700;
        else
            manifestPath.mode = hrnManifestPath.mode;

        if (hrnManifestPath.user != NULL)
            manifestPath.user = strNewZ(hrnManifestPath.user);
        else
            manifestPath.user = userName();

        if (hrnManifestPath.group != NULL)
            manifestPath.group = strNewZ(hrnManifestPath.group);
        else
            manifestPath.group = groupName();

        manifestPathAdd(manifest, &manifestPath);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnManifestTargetAdd(Manifest *const manifest, const HrnManifestTarget hrnManifestTarget)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(MANIFEST, manifest);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestTarget.name);
        FUNCTION_HARNESS_PARAM(ENUM, hrnManifestTarget.type);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestTarget.path);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestTarget.file);
        FUNCTION_HARNESS_PARAM(UINT, hrnManifestTarget.tablespaceId);
        FUNCTION_HARNESS_PARAM(STRINGZ, hrnManifestTarget.tablespaceName);
    FUNCTION_HARNESS_END();

    ASSERT(hrnManifestTarget.name != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        ManifestTarget manifestTarget =
        {
            .name = STR(hrnManifestTarget.name),
            .type = hrnManifestTarget.type,
            .tablespaceId = hrnManifestTarget.tablespaceId,
        };

        if (hrnManifestTarget.path != NULL)
            manifestTarget.path = strNewZ(hrnManifestTarget.path);

        if (hrnManifestTarget.file != NULL)
            manifestTarget.file = strNewZ(hrnManifestTarget.file);

        if (hrnManifestTarget.tablespaceName != NULL)
            manifestTarget.tablespaceName = strNewZ(hrnManifestTarget.tablespaceName);

        manifestTargetAdd(manifest, &manifestTarget);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}
