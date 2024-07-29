/***********************************************************************************************************************************
Test PostgreSQL Interface
***********************************************************************************************************************************/
#include "storage/posix/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessPostgres.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
    // Test configurations loading to initialize cfgOptFork value (PostgreSQL)
    StringList *argList = strLstNew();
    hrnCfgArgRawZ(argList, cfgOptStanza, "test");
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
    hrnCfgLoadP(cfgCmdBackup, argList);

    // *****************************************************************************************************************************
    if (testBegin("pgVersionFromStr() and pgVersionToStr()"))
    {
        TEST_ERROR(pgVersionFromStr(STRDEF("9.4.4")), AssertError, "version 9.4.4 format is invalid");
        TEST_ERROR(pgVersionFromStr(STRDEF("abc")), AssertError, "version abc format is invalid");
        TEST_ERROR(pgVersionFromStr(NULL), AssertError, "assertion 'version != NULL' failed");

        TEST_RESULT_INT(pgVersionFromStr(STRDEF("10")), PG_VERSION_10, "valid pg version 10");
        TEST_RESULT_INT(pgVersionFromStr(STRDEF("9.6")), 90600, "valid pg version 9.6");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_STR_Z(pgVersionToStr(PG_VERSION_11), "11", "infoPgVersionToString 11");
        TEST_RESULT_STR_Z(pgVersionToStr(PG_VERSION_96), "9.6", "infoPgVersionToString 9.6");
        TEST_RESULT_STR_Z(pgVersionToStr(94456), "9.44", "infoPgVersionToString 94456");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgDbIs*()"))
    {
        TEST_RESULT_BOOL(pgDbIsTemplate(STRDEF("template0")), true, "template0 is template");
        TEST_RESULT_BOOL(pgDbIsTemplate(STRDEF("template1")), true, "template1 is template");
        TEST_RESULT_BOOL(pgDbIsTemplate(STRDEF("postgres")), false, "postgres is not template");

        TEST_RESULT_BOOL(pgDbIsSystem(STRDEF("postgres")), true, "postgres is system");
        TEST_RESULT_BOOL(pgDbIsSystem(STRDEF("template0")), true, "template0 is system");
        TEST_RESULT_BOOL(pgDbIsSystem(STRDEF("app")), false, "app is not system");

        TEST_RESULT_BOOL(pgDbIsSystemId(16383), true, "16383 is system");
        TEST_RESULT_BOOL(pgDbIsSystemId(16384), false, "16384 is not system");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlVersion()"))
    {
        TEST_ERROR(
            pgControlVersion(70300), VersionNotSupportedError,
            "invalid PostgreSQL version 70300\n"
            "HINT: is this version of PostgreSQL supported?");
        TEST_RESULT_UINT(pgControlVersion(PG_VERSION_94), 942, "9.4 control version");
        TEST_RESULT_UINT(pgControlVersion(PG_VERSION_11), 1100, "11 control version");
        TEST_RESULT_UINT(pgControlVersion(PG_VERSION_17), 1300, "17 control version");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlFromBuffer() and pgControlFromFile()"))
    {
        // Sanity test to ensure PG_VERSION_MAX has been updated
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_RESULT_UINT(pgInterface[0].version, PG_VERSION_MAX, "check max version");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unknown control version");

        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(storageTest, PG_VERSION_15, 1501, .catalogVersion = 202211111);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), VersionNotSupportedError,
            "unexpected control version = 1501 and catalog version = 202211111\n"
            "HINT: is this version of PostgreSQL supported?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid CRC");

        HRN_PG_CONTROL_OVERRIDE_CRC_PUT(storageTest, PG_VERSION_11, 0xFADEFADE);

        TEST_ERROR_FMT(
            pgControlFromFile(storageTest, NULL), ChecksumError,
            "calculated pg_control checksum does not match expected value\n"
            "HINT: calculated 0x%x but expected value is 0xfadefade\n"
            "HINT: is pg_control corrupt?\n"
            "HINT: does pg_control have a different layout than expected?",
            (uint32_t)(TEST_BIG_ENDIAN() ? 0x4e206eeb : (TEST_64BIT() ? 0x4ad387b2 : 0x3ca3a1ec)));

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid CRC on force control version");

        HRN_PG_CONTROL_OVERRIDE_CRC_PUT(storageTest, PG_VERSION_13, 0xFADEFADE);

        TEST_ERROR_FMT(
            pgControlFromFile(storageTest, STRDEF(PG_VERSION_14_Z)), ChecksumError,
            "calculated pg_control checksum does not match expected value\n"
            "HINT: calculated 0x%x but expected value is 0x0\n"
            "HINT: checksum values may be misleading due to forced version scan\n"
            "HINT: is pg_control corrupt?\n"
            "HINT: does pg_control have a different layout than expected?",
            (uint32_t)(TEST_BIG_ENDIAN() ? 0x27dc2b85 : (TEST_64BIT() ? 0xa9264d94 : 0xb371302a)));

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_11, .systemId = 0xFACEFACE, .checkpoint = 0xEEFFEEFFAABBAABB, .timeline = 47,
            .walSegmentSize = 1024 * 1024);

        PgControl info = {0};
        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info v11");
        TEST_RESULT_UINT(info.systemId, 0xFACEFACE, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_11, "   check version");
        TEST_RESULT_UINT(info.catalogVersion, 201809051, "   check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xEEFFEEFFAABBAABB, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 47, "check timeline");

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_94, .walSegmentSize = 1024 * 1024);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "wal segment size is 1048576 but must be 16777216 for PostgreSQL <= 10");

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_11, .walSegmentSize = UINT_MAX); // UINT_MAX forces size to 0

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "wal segment size is 0 but must be a power of two between 1048576 and 1073741824 inclusive");

        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_11, .walSegmentSize = 1);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "wal segment size is 1 but must be a power of two between 1048576 and 1073741824 inclusive");

        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_11, .walSegmentSize = 47);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "wal segment size is 47 but must be a power of two between 1048576 and 1073741824 inclusive");

        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_17, .walSegmentSize = (unsigned int)2 * 1024 * 1024 * 1024);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "wal segment size is 2147483648 but must be a power of two between 1048576 and 1073741824 inclusive");

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_95, .pageSize = 64 * 1024);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError,
            "page size is 65536 but only 1024, 2048, 4096, 8192, 16384, and 32768 are supported");

        // -------------------------------------------------------------------------------------------------------------------------
        HRN_PG_CONTROL_PUT(storageTest, PG_VERSION_11, .pageChecksumVersion = 2);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), FormatError, "page checksum version is 2 but only 0 and 1 are valid");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check all valid page sizes");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_94, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_94),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize8);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_94, "   check version");
        TEST_RESULT_UINT(info.catalogVersion, 201409291, "   check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize8, "check page size");
        TEST_RESULT_UINT(info.pageChecksumVersion, 0, "check page checksum");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_96, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_96),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize1, .pageChecksumVersion = 1);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_96, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 201608131, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize1, "check page size");
        TEST_RESULT_UINT(info.pageChecksumVersion, 1, "check page checksum");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_12, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_12),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize2);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_12, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 201909212, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize2, "check page size");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_14, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_14),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize4);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_14, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 202107181, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize4, "check page size");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_16, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_16),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize16);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_16, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 202307071, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize16, "check page size");

        HRN_PG_CONTROL_PUT(
            storageTest, PG_VERSION_17, .systemId = 0xEFEFEFEFEF, .catalogVersion = hrnPgCatalogVersion(PG_VERSION_17),
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88, .pageSize = pgPageSize32);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_17, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 202405161, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");
        TEST_RESULT_UINT(info.pageSize, pgPageSize32, "check page size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("force control version");

        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storageTest, PG_VERSION_15, 1501, .systemId = 0xEFEFEFEFEF, .catalogVersion = 202211111,
            .checkpoint = 0xAABBAABBEEFFEEFF, .timeline = 88);

        TEST_ERROR(
            pgControlFromFile(storageTest, NULL), VersionNotSupportedError,
            "unexpected control version = 1501 and catalog version = 202211111\n"
            "HINT: is this version of PostgreSQL supported?");
        TEST_ERROR(
            pgControlFromFile(storageTest, STRDEF("99")), VersionNotSupportedError,
            "invalid PostgreSQL version 990000\n"
            "HINT: is this version of PostgreSQL supported?");

        TEST_ASSIGN(info, pgControlFromFile(storageTest, STRDEF(PG_VERSION_15_Z)), "get control info v90");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_15, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 202211111, "check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 0xAABBAABBEEFFEEFF, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 88, "check timeline");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("CRC at greater offset on force control version");

        // Start with an invalid crc but write a valid one at a greater offset to test the forced scan
        Buffer *control = hrnPgControlToBuffer(0, 0xFADEFADE, (PgControl){.version = PG_VERSION_13, .systemId = 0xAAAA0AAAA});
        size_t crcOffset = pgInterfaceVersion(PG_VERSION_13)->controlCrcOffset() + sizeof(uint32_t) * 4;
        *((uint32_t *)(bufPtr(control) + crcOffset)) = crc32cOne(bufPtrConst(control), crcOffset);

        HRN_STORAGE_PUT(storageTest, PG_PATH_GLOBAL "/" PG_FILE_PGCONTROL, control);

        TEST_ASSIGN(info, pgControlFromFile(storageTest, STRDEF(PG_VERSION_14_Z)), "get control info force v14");
        TEST_RESULT_UINT(info.systemId, 0xAAAA0AAAA, "check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_14, "check version");
        TEST_RESULT_UINT(info.catalogVersion, 202007201, "check catalog version");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Invalidate checkpoint");

        control = hrnPgControlToBuffer(0, 0, (PgControl){.version = PG_VERSION_13, .systemId = 0xAAAA0AAAA, .checkpoint = 777});

        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 777, "check checkpoint");

        TEST_RESULT_VOID(pgControlCheckpointInvalidate(control, NULL), "invalidate checkpoint");
        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 0xDEAD, "check invalid checkpoint");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("Invalidate checkpoint on force control version");

        // Start with an invalid crc but write a valid one at a greater offset to test the forced scan
        control = hrnPgControlToBuffer(
            0, 0xFADEFADE, (PgControl){.version = PG_VERSION_94, .systemId = 0xAAAA0AAAA, .checkpoint = 777});
        crcOffset = pgInterfaceVersion(PG_VERSION_94)->controlCrcOffset() + sizeof(uint32_t) * 2;
        *((uint32_t *)(bufPtr(control) + crcOffset)) = crc32One(bufPtrConst(control), crcOffset);

        info = pgControlFromBuffer(control, STRDEF(PG_VERSION_94_Z));
        TEST_RESULT_UINT(info.checkpoint, 777, "check checkpoint");

        TEST_RESULT_VOID(pgControlCheckpointInvalidate(control, STRDEF(PG_VERSION_94_Z)), "invalidate checkpoint");
        info = pgControlFromBuffer(control, STRDEF(PG_VERSION_94_Z));
        TEST_RESULT_UINT(info.checkpoint, 0xDEAD, "check invalid checkpoint");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgLsnFromStr(), pgLsnToStr(), pgLsnToWalSegment(), pg*FromWalSegment(), and pgLsnRangeToWalSegmentList()"))
    {
        TEST_RESULT_UINT(pgLsnFromStr(STRDEF("1/1")), 0x0000000100000001, "lsn to string");
        TEST_RESULT_UINT(pgLsnFromStr(STRDEF("ffffffff/ffffffff")), 0xFFFFFFFFFFFFFFFF, "lsn to string");
        TEST_RESULT_UINT(pgLsnFromStr(STRDEF("ffffffff/aaaaaaaa")), 0xFFFFFFFFAAAAAAAA, "lsn to string");

        TEST_RESULT_STR_Z(pgLsnToStr(0xFFFFFFFFAAAAAAAA), "ffffffff/aaaaaaaa", "string to lsn");
        TEST_RESULT_STR_Z(pgLsnToStr(0x0000000000000000), "0/0", "string to lsn");
        TEST_RESULT_STR_Z(pgLsnToStr(0x0000000100000002), "1/2", "string to lsn");

        TEST_RESULT_STR_Z(pgLsnToWalSegment(1, 0xFFFFFFFFAAAAAAAA, 0x1000000), "00000001FFFFFFFF000000AA", "lsn to wal segment");
        TEST_RESULT_STR_Z(pgLsnToWalSegment(1, 0xFFFFFFFFAAAAAAAA, 0x40000000), "00000001FFFFFFFF00000002", "lsn to wal segment");
        TEST_RESULT_STR_Z(pgLsnToWalSegment(1, 0xFFFFFFFF40000000, 0x40000000), "00000001FFFFFFFF00000001", "lsn to wal segment");

        TEST_RESULT_UINT(pgTimelineFromWalSegment(STRDEF("00000001FFFFFFFF000000AA")), 1, "timeline 1");
        TEST_RESULT_UINT(pgTimelineFromWalSegment(STRDEF("F000000FFFFFFFFF000000AA")), 0xF000000F, "timeline F000000F");

        TEST_RESULT_STRLST_Z(
            pgLsnRangeToWalSegmentList(
                2, pgLsnFromStr(STRDEF("1/FD000000")), pgLsnFromStr(STRDEF("2/60")), 16 * 1024 * 1024),
            "0000000200000001000000FD\n0000000200000001000000FE\n0000000200000001000000FF\n000000020000000200000000\n",
            "get range");
        TEST_RESULT_STRLST_Z(
            pgLsnRangeToWalSegmentList(
                2, pgLsnFromStr(STRDEF("A/800")), pgLsnFromStr(STRDEF("B/C0000000")), 1024 * 1024 * 1024),
            "000000020000000A00000000\n000000020000000A00000001\n000000020000000A00000002\n000000020000000A00000003\n"
            "000000020000000B00000000\n000000020000000B00000001\n000000020000000B00000002\n000000020000000B00000003\n",
            "get range >= 11/1GB");
        TEST_RESULT_STRLST_Z(
            pgLsnRangeToWalSegmentList(
                3, pgLsnFromStr(STRDEF("7/FFEFFFFF")), pgLsnFromStr(STRDEF("8/001AAAAA")), 1024 * 1024),
            "000000030000000700000FFE\n000000030000000700000FFF\n000000030000000800000000\n000000030000000800000001\n",
            "get range >= 11/1MB");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgLsnName(), pgTablespaceId(), pgWalName(), pgWalPath(), and pgXactPath()"))
    {
        TEST_RESULT_STR_Z(pgLsnName(PG_VERSION_96), "location", "check location name");
        TEST_RESULT_STR_Z(pgLsnName(PG_VERSION_10), "lsn", "check lsn name");

        TEST_RESULT_STR_Z(pgTablespaceId(PG_VERSION_94, 201409291), "PG_9.4_201409291", "check 9.4 tablespace id");
        TEST_RESULT_STR_Z(pgTablespaceId(PG_VERSION_16, 999999999), "PG_16_999999999", "check 16 tablespace id");

        TEST_RESULT_STR_Z(pgWalName(PG_VERSION_96), "xlog", "check xlog name");
        TEST_RESULT_STR_Z(pgWalName(PG_VERSION_10), "wal", "check wal name");

        TEST_RESULT_STR_Z(pgWalPath(PG_VERSION_96), "pg_xlog", "check xlog path");
        TEST_RESULT_STR_Z(pgWalPath(PG_VERSION_10), "pg_wal", "check wal path");

        TEST_RESULT_STR_Z(pgXactPath(PG_VERSION_96), "pg_clog", "check pg_clog name");
        TEST_RESULT_STR_Z(pgXactPath(PG_VERSION_10), "pg_xact", "check pg_xact name");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgPageChecksum()"))
    {
        TEST_TITLE("1KiB page checksum");
        {
            unsigned char page[pgPageSize1];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0x980F : 0x016E, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0x982A : 0x0391, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("2KiB page checksum");
        {
            unsigned char page[pgPageSize2];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0x4937 : 0xB57B, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0x48D8 : 0xB7A2, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("4KiB page checksum");
        {
            unsigned char page[pgPageSize4];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0x81DA : 0x5B9B, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0x7EB7 : 0x5BB8, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("8KiB page checksum");
        {
            unsigned char page[pgPageSize8];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0xF55E : 0x0E1C, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0xF1B9 : 0x0EC3, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("16KiB page checksum");
        {
            unsigned char page[pgPageSize16];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0xA2AD : 0x158E, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0xA548 : 0x18AD, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("32KiB page checksum");
        {
            unsigned char page[pgPageSize32];
            memset(page, 0xFF, sizeof(page));

            TEST_RESULT_UINT(
                pgPageChecksum(page, 0, sizeof(page)), TEST_BIG_ENDIAN() ? 0x7F66 : 0x5366, "check 0xFF filled page, block 0");
            TEST_RESULT_UINT(
                pgPageChecksum(page, 999, sizeof(page)), TEST_BIG_ENDIAN() ? 0x82C5 : 0x5745, "check 0xFF filled page, block 999");
        }

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid page size error");
        {
            unsigned char page[64 * 1024];
            memset(page, 0xFF, sizeof(page));

            TEST_ERROR(
                pgPageChecksum(page, 0, sizeof(page)), FormatError,
                "page size is 65536 but only 1024, 2048, 4096, 8192, 16384, and 32768 are supported");
        }
    }

    // *****************************************************************************************************************************
    if (testBegin("pgWalFromBuffer() and pgWalFromFile()"))
    {
        const String *walFile = STRDEF(TEST_PATH "/0000000F0000000F0000000F");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("unknown WAL magic");

        Buffer *result = bufNew((size_t)16 * 1024 * 1024);
        memset(bufPtr(result), 0, bufSize(result));
        bufUsedSet(result, bufSize(result));
        HRN_PG_WAL_OVERRIDE_TO_BUFFER(result, PG_VERSION_15, 777);

        TEST_ERROR(
            pgWalFromBuffer(result, NULL), VersionNotSupportedError,
            "unexpected WAL magic 777\n"
            "HINT: is this version of PostgreSQL supported?");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("invalid wal flag");

        ((PgWalCommon *)bufPtr(result))->flag = 0;

        TEST_ERROR(pgWalFromBuffer(result, NULL), FormatError, "first page header in WAL file is expected to be in long format");

        // -------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        HRN_PG_WAL_TO_BUFFER(result, PG_VERSION_11, .systemId = 0xECAFECAF, .size = PG_WAL_SEGMENT_SIZE_DEFAULT * 2);
        storagePutP(storageNewWriteP(storageTest, walFile), result);

        PgWal info = {0};
        TEST_ASSIGN(info, pgWalFromFile(walFile, storageTest, NULL), "get wal info v11");
        TEST_RESULT_UINT(info.systemId, 0xECAFECAF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_11, "   check version");
        TEST_RESULT_UINT(info.size, PG_WAL_SEGMENT_SIZE_DEFAULT * 2, "   check size");

        // -------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        HRN_PG_WAL_TO_BUFFER(result, PG_VERSION_96, .systemId = 0xEAEAEAEA, .size = PG_WAL_SEGMENT_SIZE_DEFAULT * 2);

        TEST_ERROR(
            pgWalFromBuffer(result, NULL), FormatError, "wal segment size is 33554432 but must be 16777216 for PostgreSQL <= 10");

        // -------------------------------------------------------------------------------------------------------------------------
        memset(bufPtr(result), 0, bufSize(result));
        HRN_PG_WAL_TO_BUFFER(result, PG_VERSION_94, .systemId = 0xEAEAEAEA, .size = PG_WAL_SEGMENT_SIZE_DEFAULT);
        storagePutP(storageNewWriteP(storageTest, walFile), result);

        TEST_ASSIGN(info, pgWalFromFile(walFile, storageTest, NULL), "get wal info v9.4");
        TEST_RESULT_UINT(info.systemId, 0xEAEAEAEA, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_94, "   check version");
        TEST_RESULT_UINT(info.size, PG_WAL_SEGMENT_SIZE_DEFAULT, "   check size");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("force WAL version");

        memset(bufPtr(result), 0, bufSize(result));
        HRN_PG_WAL_OVERRIDE_TO_BUFFER(result, PG_VERSION_15, 777, .systemId = 0xFAFAFAFA, .size = PG_WAL_SEGMENT_SIZE_DEFAULT);
        storagePutP(storageNewWriteP(storageTest, walFile), result);

        TEST_ERROR(
            pgWalFromBuffer(result, NULL), VersionNotSupportedError,
            "unexpected WAL magic 777\n"
            "HINT: is this version of PostgreSQL supported?");

        TEST_ASSIGN(info, pgWalFromFile(walFile, storageTest, STRDEF(PG_VERSION_15_Z)), "force wal info v15");
        TEST_RESULT_UINT(info.systemId, 0xFAFAFAFA, "check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_15, "   check version");
        TEST_RESULT_UINT(info.size, PG_WAL_SEGMENT_SIZE_DEFAULT, "   check size");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgControlToLog()"))
    {
        char logBuf[STACK_TRACE_PARAM_MAX];

        PgControl pgControl =
        {
            .version = PG_VERSION_11,
            .systemId = 0xEFEFEFEFEF,
            .walSegmentSize = 16 * 1024 * 1024,
            .pageChecksumVersion = 1,
        };

        TEST_RESULT_VOID(FUNCTION_LOG_OBJECT_FORMAT(&pgControl, pgControlToLog, logBuf, sizeof(logBuf)), "pgControlToLog");
        TEST_RESULT_Z(
            logBuf, "{version: 110000, systemId: 1030522662895, walSegmentSize: 16777216, pageChecksumVersion: 1}", "check log");
    }

    // *****************************************************************************************************************************
    if (testBegin("pgWalToLog()"))
    {
        char logBuf[STACK_TRACE_PARAM_MAX];

        PgWal pgWal =
        {
            .version = PG_VERSION_10,
            .systemId = 0xFEFEFEFEFE
        };

        TEST_RESULT_VOID(FUNCTION_LOG_OBJECT_FORMAT(&pgWal, pgWalToLog, logBuf, sizeof(logBuf)), "pgWalToLog");
        TEST_RESULT_Z(logBuf, "{version: 100000, systemId: 1095199817470}", "check log");
    }

    // *****************************************************************************************************************************
    // Test configurations loading to initialize cfgOptFork value (GPDB)
    argList = strLstNew();
    hrnCfgArgRawZ(argList, cfgOptStanza, "test");
    hrnCfgArgRawZ(argList, cfgOptFork, "GPDB");
    hrnCfgArgKeyRawZ(argList, cfgOptPgPath, 1, "/pg1");
    hrnCfgArgKeyRawZ(argList, cfgOptRepoRetentionFull, 1, "2");
    hrnCfgLoadP(cfgCmdBackup, argList);
    if (testBegin("pgTablespaceId for GPDB"))
    {
        TEST_RESULT_STR_Z(pgTablespaceId(PG_VERSION_94, 999999999), "GPDB_6_999999999", "check GPDB 6 tablespace id");
    }

    if (testBegin("pg_control with valid crc32 for Greenplum"))
    {
        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storageTest, PG_VERSION_94, 9420600, .systemId = 0xEFEFEFEFEF, .catalogVersion = 301908232,
            .pageSize = 32768, .walSegmentSize = 64 * 1024 * 1024);

        PgControl info = {0};
        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info v9.4 (Greenplum)");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_94, "   check version");
        TEST_RESULT_UINT(info.catalogVersion, 301908232, "   check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 1, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 1, "check timeline");
    }

    if (testBegin("Invalidate checkpoint for GPDB"))
    {
        PgControl info;
        Buffer *control;
        PgControl pgControl =
        {
            .version = PG_VERSION_94,
            .systemId = 0xAAAA0AAAA,
            .checkpoint = 777,
            .walSegmentSize = 64 * 1024 * 1024,
        };

        control = hrnPgControlToBuffer(0, 0, pgControl);

        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 777, "check checkpoint");

        TEST_RESULT_VOID(pgControlCheckpointInvalidate(control, NULL), "invalidate checkpoint");
        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 0xDEAD, "check invalid checkpoint");
    }

    if (testBegin("pgTablespaceId for GPDB7"))
    {
        TEST_RESULT_STR_Z(pgTablespaceId(PG_VERSION_12, 999999999), "GPDB_7_999999999", "check GPDB 7 tablespace id");
    }

    if (testBegin("pg_control with valid crc32 for GPDB7"))
    {
        HRN_PG_CONTROL_OVERRIDE_VERSION_PUT(
            storageTest, PG_VERSION_12, 12010700, .systemId = 0xEFEFEFEFEF, .catalogVersion = 302307241,
            .pageSize = 32768, .walSegmentSize = 64 * 1024 * 1024);

        PgControl info = {0};
        TEST_ASSIGN(info, pgControlFromFile(storageTest, NULL), "get control info v12 (Greenplum)");
        TEST_RESULT_UINT(info.systemId, 0xEFEFEFEFEF, "   check system id");
        TEST_RESULT_UINT(info.version, PG_VERSION_12, "   check version");
        TEST_RESULT_UINT(info.catalogVersion, 302307241, "   check catalog version");
        TEST_RESULT_UINT(info.checkpoint, 1, "check checkpoint");
        TEST_RESULT_UINT(info.timeline, 1, "check timeline");
    }

    if (testBegin("Invalidate checkpoint for GPDB7"))
    {
        PgControl info;
        Buffer *control;
        PgControl pgControl =
        {
            .version = PG_VERSION_12,
            .systemId = 0xAAAA0AAAA,
            .checkpoint = 777,
            .walSegmentSize = 64 * 1024 * 1024,
        };

        control = hrnPgControlToBuffer(0, 0, pgControl);

        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 777, "check checkpoint");

        TEST_RESULT_VOID(pgControlCheckpointInvalidate(control, NULL), "invalidate checkpoint");
        info = pgControlFromBuffer(control, NULL);
        TEST_RESULT_UINT(info.checkpoint, 0xDEAD, "check invalid checkpoint");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
