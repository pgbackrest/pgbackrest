/***********************************************************************************************************************************
Ensure that symlinks do not point to the same file, directory, or subdirectory of another link

There are two implementations: manifestLinkCheck(), which is externed, and manifestLinkCheckOne(), which is intended to be
used internally during processing. manifestLinkCheck() works simply by calling manifestLinkCheckOne() for every link in the target
list. manifestLinkCheckOne() is optimized to work quickly on a single link.
***********************************************************************************************************************************/
// Data needed in link list
typedef struct ManifestLinkCheckItem
{
    const String *path;                                             // Link destination path terminated with /
    const String *file;                                             // Link file if a file link
    unsigned int targetIdx;                                         // Index of target used for error messages
} ManifestLinkCheckItem;

// Persistent data needed during processing of manifestLinkCheck/One()
typedef struct ManifestLinkCheck
{
    const String *basePath;                                         // Base data path (initialized on first call)
    List *linkList;                                                 // Current list of link destination paths
} ManifestLinkCheck;

// Helper to initialize the link data
static ManifestLinkCheck
manifestLinkCheckInit(void)
{
    FUNCTION_TEST_VOID();
    FUNCTION_AUDIT_STRUCT();
    FUNCTION_TEST_RETURN_TYPE(
        ManifestLinkCheck, (ManifestLinkCheck){.linkList = lstNewP(sizeof(ManifestLinkCheckItem), .comparator = lstComparatorStr)});
}

// Helper to check a single link specified by targetIdx
static void
manifestLinkCheckOne(const Manifest *const this, ManifestLinkCheck *const linkCheck, const unsigned int targetIdx)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(MANIFEST, this);
        FUNCTION_LOG_PARAM_P(VOID, linkCheck);
        FUNCTION_LOG_PARAM(UINT, targetIdx);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);
    ASSERT(linkCheck != NULL);
    ASSERT(linkCheck->linkList != NULL);
    ASSERT(targetIdx < manifestTargetTotal(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const ManifestTarget *const target1 = manifestTarget(this, targetIdx);

        // Only check link targets
        if (target1->type == manifestTargetTypeLink)
        {
            // Create link destination path for comparison with other paths. It must end in / so subpaths can be detected without
            // matching valid partial path names at the end of the path.
            const String *path = NULL;

            MEM_CONTEXT_BEGIN(lstMemContext(linkCheck->linkList))
            {
                path = strNewFmt("%s/", strZ(manifestTargetPath(this, target1)));

                // Get base bath
                if (linkCheck->basePath == NULL)
                {
                    linkCheck->basePath = strNewFmt(
                        "%s/", strZ(manifestTargetPath(this, manifestTargetFind(this, MANIFEST_TARGET_PGDATA_STR))));
                }
            }
            MEM_CONTEXT_END();

            // Check that link destination is not in base data path
            if (strBeginsWith(path, linkCheck->basePath))
            {
                THROW_FMT(
                    LinkDestinationError,
                    "link '%s' destination '%s' is in PGDATA",
                    strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)));
            }

            // Check if the link destination path already exists
            const ManifestLinkCheckItem *const link = lstFind(linkCheck->linkList, &path);

            if (link != NULL)
            {
                // If both links are files make sure they don't link to the same file
                if (target1->file != NULL && link->file != NULL)
                {
                    if (strEq(target1->file, link->file))
                    {
                        const ManifestTarget *const target2 = manifestTarget(this, link->targetIdx);

                        THROW_FMT(
                            LinkDestinationError,
                            "link '%s' (%s/%s) destination is the same file as link '%s' (%s/%s)",
                            strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)), strZ(target1->file),
                            strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)), strZ(target2->file));
                    }
                }
                // Else error because one of the links is a path and cannot link to the same path as another file/path link
                else
                {
                    const ManifestTarget *const target2 = manifestTarget(this, link->targetIdx);

                    THROW_FMT(
                        LinkDestinationError,
                        "link '%s' (%s) destination is the same directory as link '%s' (%s)",
                        strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)),
                        strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)));
                }
            }
            // Else add to the link list and check against other links
            else
            {
                // Add the link destination path and sort
                lstAdd(linkCheck->linkList, &(ManifestLinkCheckItem){.path = path, .file = target1->file, .targetIdx = targetIdx});
                lstSort(linkCheck->linkList, sortOrderAsc);

                // Find the path in the sorted list
                const unsigned int linkIdx = lstFindIdx(linkCheck->linkList, &path);
                ASSERT(linkIdx != LIST_NOT_FOUND);

                // Check path links against other links (file links have already been checked)
                if (target1->file == NULL)
                {
                    // Check the link destination path to be sure it is not a subpath of a prior link destination path
                    for (unsigned int priorLinkIdx = linkIdx - 1; (int)priorLinkIdx >= 0; priorLinkIdx--)
                    {
                        const ManifestLinkCheckItem *const priorLink = lstGet(linkCheck->linkList, priorLinkIdx);

                        // Skip file links since they are allowed to be in the same path with each other and in the parent path of a
                        // linked destination path.
                        if (priorLink->file != NULL)
                            continue;

                        if (strBeginsWith(path, priorLink->path))
                        {
                            const ManifestTarget *const target2 = manifestTarget(this, priorLink->targetIdx);

                            THROW_FMT(
                                LinkDestinationError,
                                "link '%s' (%s) destination is a subdirectory of link '%s' (%s)",
                                strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)),
                                strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)));
                        }

                        // Stop once the first prior path link has been checked since it must be a parent (if there is one)
                        break;
                    }

                    // Check the link destination path to be sure it is not a parent path of a subsequent link destination path
                    for (unsigned int nextLinkIdx = linkIdx + 1; nextLinkIdx < lstSize(linkCheck->linkList); nextLinkIdx++)
                    {
                        const ManifestLinkCheckItem *const nextLink = lstGet(linkCheck->linkList, nextLinkIdx);

                        // Skip file links since they are allowed to be in the same path with each other and in the parent path of a
                        // linked destination path.
                        if (nextLink->file != NULL)
                            continue;

                        if (strBeginsWith(nextLink->path, path))
                        {
                            const ManifestTarget *const target2 = manifestTarget(this, nextLink->targetIdx);

                            THROW_FMT(
                                LinkDestinationError,
                                "link '%s' (%s) destination is a subdirectory of link '%s' (%s)",
                                strZ(manifestPathPg(target2->name)), strZ(manifestTargetPath(this, target2)),
                                strZ(manifestPathPg(target1->name)), strZ(manifestTargetPath(this, target1)));
                        }

                        // Stop once the first next path link has been checked since it must be a subpath (if there is one)
                        break;
                    }
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}

FN_EXTERN void
manifestLinkCheck(const Manifest *const this)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(MANIFEST, this);
    FUNCTION_LOG_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Check all links
        ManifestLinkCheck linkCheck = manifestLinkCheckInit();

        for (unsigned int targetIdx = 0; targetIdx < manifestTargetTotal(this); targetIdx++)
            manifestLinkCheckOne(this, &linkCheck, targetIdx);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
