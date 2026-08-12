/***********************************************************************************************************************************
PostgreSQL Version Interface

Macros for building version-specific functions that interface with the types in version.vendor.h. Due to the way PostgreSQL types
evolve over time, this seems to be the easiest way to extract information from them.

These macros should be kept as simple as possible, with most of the logic contained in postgres/interface.c.
***********************************************************************************************************************************/
#include "postgres/interface/version.vendor.h"
#include "postgres/version.h"

/***********************************************************************************************************************************
Values that vary by version

Each is captured as a constant named for the version so that the interface struct, which is built after every version has
undefined what it defined, can still name it. A define cannot do this because it is expanded where it is used rather than where
it is made, by which time the value it was given is gone.

One macro per value so that a value which does not vary between two versions is captured once for both, the same way a function
that does not vary is rendered once for both.
***********************************************************************************************************************************/
// Control version that pg_control matches for this version of PostgreSQL
#define PG_INTERFACE_VALUE_CONTROL_VERSION(version)                                                                                \
    enum {pgInterfaceControlVersion##version = PG_CONTROL_VERSION}

// Catalog version that pg_control matches for this version of PostgreSQL
#define PG_INTERFACE_VALUE_CATALOG_VERSION(version)                                                                                \
    enum {pgInterfaceCatalogVersion##version = CATALOG_VERSION_NO}

// Magic that the WAL header matches for this version of PostgreSQL
#define PG_INTERFACE_VALUE_WAL_MAGIC(version)                                                                                      \
    enum {pgInterfaceWalMagic##version = XLOG_PAGE_MAGIC}

// Offset of the crc in pg_control, which is also the length of the part the crc is calculated over since the crc is last
#define PG_INTERFACE_VALUE_CONTROL_CRC_OFFSET(version)                                                                             \
    enum {pgInterfaceControlCrcOffset##version = offsetof(ControlFileData, crc)}

/***********************************************************************************************************************************
Determine whether pg_control matches an interface

Rendered once after the versions, in whichever form fits the versions that were rendered. The range form is rendered when one of
them has not been released, since only then is there an interface accepting more than a single catalog version. Rendering the form
that is used means there is never a comparison that no supported version can fail.

An unreleased version accepts any catalog version until the end of the year the one it was built with encodes.
***********************************************************************************************************************************/
#define PG_INTERFACE_CONTROL_MATCH()                                                                                               \
    static bool                                                                                                                    \
    pgInterfaceControlMatch(const PgInterface *const interface, const PgControlCommon *const control)                              \
    {                                                                                                                              \
        return control->controlVersion == interface->controlVersion && control->catalogVersion == interface->catalogVersion;       \
    }

#define PG_INTERFACE_CONTROL_MATCH_RANGE()                                                                                         \
    static bool                                                                                                                    \
    pgInterfaceControlMatch(const PgInterface *const interface, const PgControlCommon *const control)                              \
    {                                                                                                                              \
        if (control->controlVersion != interface->controlVersion)                                                                  \
            return false;                                                                                                          \
                                                                                                                                   \
        if (interface->unreleased)                                                                                                 \
        {                                                                                                                          \
            return                                                                                                                 \
                control->catalogVersion >= interface->catalogVersion &&                                                            \
                control->catalogVersion < (interface->catalogVersion / 100000 + 1) * 100000;                                       \
        }                                                                                                                          \
                                                                                                                                   \
        return control->catalogVersion == interface->catalogVersion;                                                               \
    }

/***********************************************************************************************************************************
Read the version specific pg_control into a general data structure
***********************************************************************************************************************************/
#define PG_INTERFACE_CONTROL(version)                                                                                              \
    static PgControl                                                                                                               \
    pgInterfaceControl##version(const uint8_t *controlFile)                                                                        \
    {                                                                                                                              \
        ASSERT(controlFile != NULL);                                                                                               \
                                                                                                                                   \
        return (PgControl)                                                                                                         \
        {                                                                                                                          \
            .systemId = ((const ControlFileData *)controlFile)->system_identifier,                                                 \
            .catalogVersion = ((const ControlFileData *)controlFile)->catalog_version_no,                                          \
            .checkpoint = ((const ControlFileData *)controlFile)->checkPoint,                                                      \
            .timeline = ((const ControlFileData *)controlFile)->checkPointCopy.ThisTimeLineID,                                     \
            .pageSize = ((const ControlFileData *)controlFile)->blcksz,                                                            \
            .walSegmentSize = ((const ControlFileData *)controlFile)->xlog_seg_size,                                               \
            .pageChecksumVersion = ((const ControlFileData *)controlFile)->data_checksum_version,                                  \
        };                                                                                                                         \
    }

/***********************************************************************************************************************************
Invalidate control checkpoint. PostgreSQL skips the first segment so any LSN in that segment is invalid.
***********************************************************************************************************************************/
#define PG_INTERFACE_CONTROL_CHECKPOINT_INVALIDATE(version)                                                                        \
    static void                                                                                                                    \
    pgInterfaceControlCheckpointInvalidate##version(uint8_t *const controlFile)                                                    \
    {                                                                                                                              \
        ((ControlFileData *)controlFile)->checkPoint = PG_CONTROL_CHECKPOINT_INVALID;                                              \
    }

/***********************************************************************************************************************************
Read the version specific WAL header into a general data structure
***********************************************************************************************************************************/
#define PG_INTERFACE_WAL(version)                                                                                                  \
    static PgWal                                                                                                                   \
    pgInterfaceWal##version(const uint8_t *walFile)                                                                                \
    {                                                                                                                              \
        ASSERT(walFile != NULL);                                                                                                   \
                                                                                                                                   \
        return (PgWal)                                                                                                             \
        {                                                                                                                          \
            .systemId = ((const XLogLongPageHeaderData *)walFile)->xlp_sysid,                                                      \
            .size = ((const XLogLongPageHeaderData *)walFile)->xlp_seg_size,                                                       \
        };                                                                                                                         \
    }
