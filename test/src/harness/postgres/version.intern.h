/***********************************************************************************************************************************
Harness for PostgreSQL Interface

Macros for building version-specific functions that write the pg_control and WAL a test needs. This is the write side of the
interface the code reads with postgres/interface/version.intern.h.

The types are the ones that interface declares, which the harness reaches by shimming it rather than declaring them again, so a type
may only be named here if it is named there. Everything else the macros use is included by the module that expands them.
***********************************************************************************************************************************/

/***********************************************************************************************************************************
Create a pg_control file
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE_CONTROL(version)                                                                                          \
    static void                                                                                                                    \
    hrnPgInterfaceControl##version(                                                                                                \
        const unsigned int controlVersion, const unsigned int crc, const PgControl pgControl, uint8_t *const buffer)               \
    {                                                                                                                              \
        ASSERT(buffer != NULL);                                                                                                    \
                                                                                                                                   \
        *(ControlFileData *)buffer = (ControlFileData)                                                                             \
        {                                                                                                                          \
            .system_identifier = pgControl.systemId,                                                                               \
            .pg_control_version = controlVersion,                                                                                  \
            .catalog_version_no = pgControl.catalogVersion,                                                                        \
            .checkPoint = pgControl.checkpoint,                                                                                    \
            .checkPointCopy =                                                                                                      \
            {                                                                                                                      \
                .ThisTimeLineID = pgControl.timeline,                                                                              \
            },                                                                                                                     \
            .blcksz = pgControl.pageSize,                                                                                          \
            .xlog_seg_size = pgControl.walSegmentSize,                                                                             \
            .data_checksum_version = pgControl.pageChecksumVersion,                                                                \
        };                                                                                                                         \
                                                                                                                                   \
        ((ControlFileData *)buffer)->crc = crc == 0 ? crc32cOne(buffer, offsetof(ControlFileData, crc)) : crc;                     \
    }

/***********************************************************************************************************************************
Create a WAL file
***********************************************************************************************************************************/
#define HRN_PG_INTERFACE_WAL(version)                                                                                              \
    static void                                                                                                                    \
    hrnPgInterfaceWal##version(const unsigned int magic, const PgWal pgWal, uint8_t *const buffer)                                 \
    {                                                                                                                              \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_magic = (uint16)magic;                                                         \
        ((XLogLongPageHeaderData *)buffer)->std.xlp_info = PG_WAL_LONG_HEADER;                                                     \
        ((XLogLongPageHeaderData *)buffer)->xlp_sysid = pgWal.systemId;                                                            \
        ((XLogLongPageHeaderData *)buffer)->xlp_seg_size = pgWal.size;                                                             \
    }
