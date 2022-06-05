/***********************************************************************************************************************************
Test Build PostgreSQL Interface
***********************************************************************************************************************************/
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
static void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    // Create default storage object for testing
    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    // *****************************************************************************************************************************
    if (testBegin("bldPgParse() and bldPgRender()"))
    {
        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse errors");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/postgres/postgres.yaml",
            "bogus: value\n");

        TEST_ERROR(bldPgParse(storageTest), FormatError, "unknown postgres definition 'bogus'");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/postgres/postgres.yaml",
            "version:\n"
            "  - 11:\n"
            "      bogus: value");

        TEST_ERROR(bldPgParse(storageTest), FormatError, "unknown postgres definition 'bogus'");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("parse and render postgres");

        HRN_STORAGE_PUT_Z(
            storageTest, "src/build/postgres/postgres.yaml",
            "version:\n"
            "  - 9.0\n"
            "  - 11:\n"
            "      release: false\n");

        TEST_RESULT_VOID(bldPgRender(storageTest, bldPgParse(storageTest)), "parse and render");

        // -------------------------------------------------------------------------------------------------------------------------
        TEST_TITLE("check interface.auto.c.inc");

        TEST_STORAGE_GET(
            storageTest,
            "postgres/interface.auto.c.inc",
            COMMENT_BLOCK_BEGIN "\n"
            "PostgreSQL Interface\n"
            "\n"
            "Automatically generated by 'make build-postgres' -- do not modify directly.\n"
            COMMENT_BLOCK_END "\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "PostgreSQL 11 interface\n"
            COMMENT_BLOCK_END "\n"
            "#define PG_VERSION                                                  PG_VERSION_11\n"
            "\n"
            "#define CheckPoint                                                  CheckPoint_11\n"
            "#define ControlFileData                                             ControlFileData_11\n"
            "#define DBState                                                     DBState_11\n"
            "#define DB_STARTUP                                                  DB_STARTUP_11\n"
            "#define DB_SHUTDOWNED                                               DB_SHUTDOWNED_11\n"
            "#define DB_SHUTDOWNED_IN_RECOVERY                                   DB_SHUTDOWNED_IN_RECOVERY_11\n"
            "#define DB_SHUTDOWNING                                              DB_SHUTDOWNING_11\n"
            "#define DB_IN_CRASH_RECOVERY                                        DB_IN_CRASH_RECOVERY_11\n"
            "#define DB_IN_ARCHIVE_RECOVERY                                      DB_IN_ARCHIVE_RECOVERY_11\n"
            "#define DB_IN_PRODUCTION                                            DB_IN_PRODUCTION_11\n"
            "#define FullTransactionId                                           FullTransactionId_11\n"
            "#define int64                                                       int64_11\n"
            "#define MultiXactId                                                 MultiXactId_11\n"
            "#define MultiXactOffset                                             MultiXactOffset_11\n"
            "#define Oid                                                         Oid_11\n"
            "#define pg_crc32                                                    pg_crc32_11\n"
            "#define pg_crc32c                                                   pg_crc32c_11\n"
            "#define pg_time_t                                                   pg_time_t_11\n"
            "#define TimeLineID                                                  TimeLineID_11\n"
            "#define XLogLongPageHeaderData                                      XLogLongPageHeaderData_11\n"
            "#define XLogPageHeaderData                                          XLogPageHeaderData_11\n"
            "#define XLogRecPtr                                                  XLogRecPtr_11\n"
            "\n"
            "#define CATALOG_VERSION_NO_MAX\n"
            "\n"
            "#include \"postgres/interface/version.intern.h\"\n"
            "\n"
            "PG_INTERFACE_CONTROL_IS(110);\n"
            "PG_INTERFACE_CONTROL(110);\n"
            "PG_INTERFACE_CONTROL_VERSION(110);\n"
            "PG_INTERFACE_WAL_IS(110);\n"
            "PG_INTERFACE_WAL(110);\n"
            "\n"
            "#undef CheckPoint\n"
            "#undef ControlFileData\n"
            "#undef DBState\n"
            "#undef DB_STARTUP\n"
            "#undef DB_SHUTDOWNED\n"
            "#undef DB_SHUTDOWNED_IN_RECOVERY\n"
            "#undef DB_SHUTDOWNING\n"
            "#undef DB_IN_CRASH_RECOVERY\n"
            "#undef DB_IN_ARCHIVE_RECOVERY\n"
            "#undef DB_IN_PRODUCTION\n"
            "#undef FullTransactionId\n"
            "#undef int64\n"
            "#undef MultiXactId\n"
            "#undef MultiXactOffset\n"
            "#undef Oid\n"
            "#undef pg_crc32\n"
            "#undef pg_crc32c\n"
            "#undef pg_time_t\n"
            "#undef TimeLineID\n"
            "#undef XLogLongPageHeaderData\n"
            "#undef XLogPageHeaderData\n"
            "#undef XLogRecPtr\n"
            "\n"
            "#undef CATALOG_VERSION_NO\n"
            "#undef CATALOG_VERSION_NO_MAX\n"
            "#undef PG_CONTROL_VERSION\n"
            "#undef PG_VERSION\n"
            "#undef XLOG_PAGE_MAGIC\n"
            "\n"
            "#undef PG_INTERFACE_CONTROL_IS\n"
            "#undef PG_INTERFACE_CONTROL\n"
            "#undef PG_INTERFACE_CONTROL_VERSION\n"
            "#undef PG_INTERFACE_WAL_IS\n"
            "#undef PG_INTERFACE_WAL\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "PostgreSQL 9.0 interface\n"
            COMMENT_BLOCK_END "\n"
            "#define PG_VERSION                                                  PG_VERSION_90\n"
            "\n"
            "#define CheckPoint                                                  CheckPoint_90\n"
            "#define ControlFileData                                             ControlFileData_90\n"
            "#define DBState                                                     DBState_90\n"
            "#define DB_STARTUP                                                  DB_STARTUP_90\n"
            "#define DB_SHUTDOWNED                                               DB_SHUTDOWNED_90\n"
            "#define DB_SHUTDOWNED_IN_RECOVERY                                   DB_SHUTDOWNED_IN_RECOVERY_90\n"
            "#define DB_SHUTDOWNING                                              DB_SHUTDOWNING_90\n"
            "#define DB_IN_CRASH_RECOVERY                                        DB_IN_CRASH_RECOVERY_90\n"
            "#define DB_IN_ARCHIVE_RECOVERY                                      DB_IN_ARCHIVE_RECOVERY_90\n"
            "#define DB_IN_PRODUCTION                                            DB_IN_PRODUCTION_90\n"
            "#define FullTransactionId                                           FullTransactionId_90\n"
            "#define int64                                                       int64_90\n"
            "#define MultiXactId                                                 MultiXactId_90\n"
            "#define MultiXactOffset                                             MultiXactOffset_90\n"
            "#define Oid                                                         Oid_90\n"
            "#define pg_crc32                                                    pg_crc32_90\n"
            "#define pg_crc32c                                                   pg_crc32c_90\n"
            "#define pg_time_t                                                   pg_time_t_90\n"
            "#define TimeLineID                                                  TimeLineID_90\n"
            "#define XLogLongPageHeaderData                                      XLogLongPageHeaderData_90\n"
            "#define XLogPageHeaderData                                          XLogPageHeaderData_90\n"
            "#define XLogRecPtr                                                  XLogRecPtr_90\n"
            "\n"
            "#include \"postgres/interface/version.intern.h\"\n"
            "\n"
            "PG_INTERFACE_CONTROL_IS(090);\n"
            "PG_INTERFACE_CONTROL(090);\n"
            "PG_INTERFACE_CONTROL_VERSION(090);\n"
            "PG_INTERFACE_WAL_IS(090);\n"
            "PG_INTERFACE_WAL(090);\n"
            "\n"
            "#undef CheckPoint\n"
            "#undef ControlFileData\n"
            "#undef DBState\n"
            "#undef DB_STARTUP\n"
            "#undef DB_SHUTDOWNED\n"
            "#undef DB_SHUTDOWNED_IN_RECOVERY\n"
            "#undef DB_SHUTDOWNING\n"
            "#undef DB_IN_CRASH_RECOVERY\n"
            "#undef DB_IN_ARCHIVE_RECOVERY\n"
            "#undef DB_IN_PRODUCTION\n"
            "#undef FullTransactionId\n"
            "#undef int64\n"
            "#undef MultiXactId\n"
            "#undef MultiXactOffset\n"
            "#undef Oid\n"
            "#undef pg_crc32\n"
            "#undef pg_crc32c\n"
            "#undef pg_time_t\n"
            "#undef TimeLineID\n"
            "#undef XLogLongPageHeaderData\n"
            "#undef XLogPageHeaderData\n"
            "#undef XLogRecPtr\n"
            "\n"
            "#undef CATALOG_VERSION_NO\n"
            "#undef CATALOG_VERSION_NO_MAX\n"
            "#undef PG_CONTROL_VERSION\n"
            "#undef PG_VERSION\n"
            "#undef XLOG_PAGE_MAGIC\n"
            "\n"
            "#undef PG_INTERFACE_CONTROL_IS\n"
            "#undef PG_INTERFACE_CONTROL\n"
            "#undef PG_INTERFACE_CONTROL_VERSION\n"
            "#undef PG_INTERFACE_WAL_IS\n"
            "#undef PG_INTERFACE_WAL\n"
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "PostgreSQL interface struct\n"
            COMMENT_BLOCK_END "\n"
            "static const PgInterface pgInterface[] =\n"
            "{\n"
            "    {\n"
            "        .version = PG_VERSION_11,\n"
            "\n"
            "        .controlIs = pgInterfaceControlIs110,\n"
            "        .control = pgInterfaceControl110,\n"
            "        .controlVersion = pgInterfaceControlVersion110,\n"
            "\n"
            "        .walIs = pgInterfaceWalIs110,\n"
            "        .wal = pgInterfaceWal110,\n"
            "    },\n"
            "    {\n"
            "        .version = PG_VERSION_90,\n"
            "\n"
            "        .controlIs = pgInterfaceControlIs090,\n"
            "        .control = pgInterfaceControl090,\n"
            "        .controlVersion = pgInterfaceControlVersion090,\n"
            "\n"
            "        .walIs = pgInterfaceWalIs090,\n"
            "        .wal = pgInterfaceWal090,\n"
            "    },\n"
            "};\n");
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
