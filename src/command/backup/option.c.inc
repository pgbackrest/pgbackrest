/**********************************************************************************************************************************
Build block incremental maps
***********************************************************************************************************************************/
// Size map. Block size is increased when the block map would be larger than a single block. The break can be calculated with this
// formula: [block size in KiB] / (1024 / [block size in KiB] * [checksum size]) * 1073741824.
static const ManifestBlockIncrSizeMap manifestBlockIncrSizeMapDefault[] =
{
    {.fileSize = 914 * 1024 * 1024, .blockSize = 88 * 1024},
    {.fileSize = 740 * 1024 * 1024, .blockSize = 80 * 1024},
    {.fileSize = 585 * 1024 * 1024, .blockSize = 72 * 1024},
    {.fileSize = 448 * 1024 * 1024, .blockSize = 64 * 1024},
    {.fileSize = 329 * 1024 * 1024, .blockSize = 56 * 1024},
    {.fileSize = 228 * 1024 * 1024, .blockSize = 48 * 1024},
    {.fileSize = 146 * 1024 * 1024, .blockSize = 40 * 1024},
    {.fileSize = 96 * 1024 * 1024, .blockSize = 32 * 1024},
    {.fileSize = 43 * 1024 * 1024, .blockSize = 24 * 1024},
    {.fileSize = 11 * 1024 * 1024, .blockSize = 16 * 1024},
    {.fileSize = 16 * 1024, .blockSize = 8 * 1024},
};

// Age map
static const ManifestBlockIncrAgeMap manifestBlockIncrAgeMapDefault[] =
{
    {.fileAge = 4 * 7 * SEC_PER_DAY, .blockMultiplier = 0},
    {.fileAge = 2 * 7 * SEC_PER_DAY, .blockMultiplier = 4},
    {.fileAge = 7 * SEC_PER_DAY, .blockMultiplier = 2},
};

// Checksum size map
static const ManifestBlockIncrChecksumSizeMap manifestBlockIncrChecksumSizeMapDefault[] =
{
    {.blockSize = 4 * 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 6},
    {.blockSize = 2 * 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 5},
    {.blockSize = 1024 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 4},
    {.blockSize = 512 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 3},
    {.blockSize = 128 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 2},
    {.blockSize = 32 * 1024, .checksumSize = BLOCK_INCR_CHECKSUM_SIZE_MIN + 1},
};

// All maps
static const ManifestBlockIncrMap manifestBlockIncrMap =
{
    .sizeMap = LSTDEF(manifestBlockIncrSizeMapDefault),
    .ageMap = LSTDEF(manifestBlockIncrAgeMapDefault),
    .checksumSizeMap = LSTDEF(manifestBlockIncrChecksumSizeMapDefault),
};

// Convert map size
static unsigned int
backupBlockIncrMapSize(const ConfigOption optionId, const unsigned int optionKeyIdx, const String *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(STRING, value);
    FUNCTION_TEST_END();

    int64_t result;

    TRY_BEGIN()
    {
        result = cfgParseSize(value);

        // Error if value is out of range (no need for an error message since that will be generated in the catch block)
        if (result <= 0 || result > UINT_MAX)
            THROW(OptionInvalidValueError, "");
    }
    CATCH_ANY()
    {
        THROW_FMT(
            OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(value),
            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
    }
    TRY_END();

    ASSERT(result > 0 && result < UINT_MAX);

    FUNCTION_TEST_RETURN(UINT, (unsigned int)result);
}

// Convert map checksum size
static unsigned int
backupBlockIncrMapChecksumSize(const ConfigOption optionId, const unsigned int optionKeyIdx, const Variant *const value)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(ENUM, optionId);
        FUNCTION_TEST_PARAM(UINT, optionKeyIdx);
        FUNCTION_TEST_PARAM(VARIANT, value);
    FUNCTION_TEST_END();

    unsigned int result = 0;

    TRY_BEGIN()
    {
        result = varUIntForce(value);
    }
    CATCH_ANY()
    {
    }
    TRY_END();

    if (result < BLOCK_INCR_CHECKSUM_SIZE_MIN)
    {
        THROW_FMT(
            OptionInvalidValueError, "'%s' is not valid for '%s' option", strZ(varStr(value)),
            cfgParseOptionKeyIdxName(optionId, optionKeyIdx));
    }

    FUNCTION_TEST_RETURN(UINT, result);
}

static ManifestBlockIncrMap
backupBlockIncrMap(void)
{
    FUNCTION_TEST_VOID();

    FUNCTION_AUDIT_HELPER();

    ManifestBlockIncrMap result = manifestBlockIncrMap;

    if (cfgOptionBool(cfgOptRepoBlock))
    {
        // Build size map
        const KeyValue *const manifestBlockIncrSizeKv = cfgOptionKvNull(cfgOptRepoBlockSizeMap);

        if (manifestBlockIncrSizeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrSizeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrSizeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrSizeMap manifestBuildBlockIncrSizeMap =
                {
                    .fileSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockSizeMap), varStr(mapKey)),
                    .blockSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockSizeMap),
                        varStr(kvGet(manifestBlockIncrSizeKv, mapKey))),
                };

                lstAdd(map, &manifestBuildBlockIncrSizeMap);
            }

            lstSort(map, sortOrderDesc);

            result.sizeMap = map;
        }

        // Build age map
        const KeyValue *const manifestBlockIncrAgeKv = cfgOptionKvNull(cfgOptRepoBlockAgeMap);

        if (manifestBlockIncrAgeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrAgeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrAgeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrAgeMap manifestBuildBlockIncrAgeMap =
                {
                    .fileAge = (unsigned int)(varUIntForce(mapKey) * SEC_PER_DAY),
                    .blockMultiplier = varUIntForce(kvGet(manifestBlockIncrAgeKv, mapKey)),
                };

                lstAdd(map, &manifestBuildBlockIncrAgeMap);
            }

            lstSort(map, sortOrderDesc);

            result.ageMap = map;
        }

        // Build checksum size map
        const KeyValue *const manifestBlockIncrChecksumSizeKv = cfgOptionKvNull(cfgOptRepoBlockChecksumSizeMap);

        if (manifestBlockIncrChecksumSizeKv != NULL)
        {
            List *const map = lstNewP(sizeof(ManifestBlockIncrChecksumSizeMap), .comparator = lstComparatorUInt);
            const VariantList *const mapKeyList = kvKeyList(manifestBlockIncrChecksumSizeKv);

            for (unsigned int mapKeyIdx = 0; mapKeyIdx < varLstSize(mapKeyList); mapKeyIdx++)
            {
                const Variant *const mapKey = varLstGet(mapKeyList, mapKeyIdx);

                ManifestBlockIncrChecksumSizeMap manifestBuildBlockIncrChecksumSizeMap =
                {
                    .blockSize = backupBlockIncrMapSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockChecksumSizeMap), varStr(mapKey)),
                    .checksumSize = backupBlockIncrMapChecksumSize(
                        cfgOptRepoBlockSizeMap, cfgOptionIdxDefault(cfgOptRepoBlockChecksumSizeMap),
                        kvGet(manifestBlockIncrChecksumSizeKv, mapKey)),
                };

                lstAdd(map, &manifestBuildBlockIncrChecksumSizeMap);
            }

            lstSort(map, sortOrderDesc);

            result.checksumSizeMap = map;
        }
    }

    FUNCTION_TEST_RETURN_TYPE(ManifestBlockIncrMap, result);
}
