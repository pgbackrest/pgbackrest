/***********************************************************************************************************************************
Host Harness
***********************************************************************************************************************************/
#ifndef TEST_COMMON_HARNESS_HOST_H
#define TEST_COMMON_HARNESS_HOST_H

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct HrnHost HrnHost;

#include "common/type/object.h"
#include "common/type/string.h"
#include "postgres/client.h"

#include "common/harnessTest.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Host constants
#define HRN_HOST_AZURE                                              STORAGE_AZURE_TYPE
#define HRN_HOST_AZURE_Z                                            "azure"
#define HRN_HOST_GCS                                                STORAGE_GCS_TYPE
#define HRN_HOST_GCS_Z                                              "gcs"
#define HRN_HOST_POSIX                                              STORAGE_POSIX_TYPE
#define HRN_HOST_POSIX_Z                                            "posix"
#define HRN_HOST_PG1                                                STRID6("pg1", 0x1d1d01)
#define HRN_HOST_PG1_Z                                              "pg1"
#define HRN_HOST_PG2                                                STRID5("pg2", 0x70f00)
#define HRN_HOST_PG2_Z                                              "pg2"
#define HRN_HOST_REPO                                               STRID5("repo", 0x7c0b20)
#define HRN_HOST_REPO_Z                                             "repo"
#define HRN_HOST_S3                                                 STORAGE_S3_TYPE
#define HRN_HOST_S3_Z                                               "s3"
#define HRN_HOST_SFTP                                               STORAGE_SFTP_TYPE
#define HRN_HOST_SFTP_Z                                             "sftp"

// Test stanza
#define HRN_STANZA                                                  "test"

/***********************************************************************************************************************************
Test
***********************************************************************************************************************************/
typedef struct HrnHostTestDefine
{
    const char *pg;                                                 // PostgreSQL version
    const char *repo;                                               // Host acting as repository
    bool tls;                                                       // Use TLS instead of SSH for remote protocol?
    const char *stg;                                                // Storage type
    bool enc;                                                       // Encryption
    const char *cmp;                                                // Compression type
    unsigned int rt;                                                // Repository total
    bool bnd;                                                       // Bundling enabled?
    bool bi;                                                        // Block incremental enabled?
    bool fi;                                                        // Full/incr backup?
} HrnHostTestDefine;

/***********************************************************************************************************************************
Constructor
***********************************************************************************************************************************/
typedef struct HrnHostNewParam
{
    VAR_PARAM_HEADER;
    const String *entryPoint;                                       // Container entry point
    const String *user;                                             // Container user
    const String *option;                                           // Options (e.g. -v)
    const String *param;                                            // Parameters (passed to the container command)
    bool noUpdateHosts;                                             // Do not update /etc/hosts file on this host
    const String *dataPath;                                         // Path where container data is stored
    bool pgStandby;                                                 // Pg standby?
    bool isPg;                                                      // Is a pg host?
    bool isRepo;                                                    // Is a repo host?
} HrnHostNewParam;

#define hrnHostNewP(name, container, image, ...)                                                                                   \
    hrnHostNew(name, container, image, (HrnHostNewParam){VAR_PARAM_INIT, __VA_ARGS__})

HrnHost *hrnHostNew(StringId id, const String *container, const String *image, HrnHostNewParam param);

/***********************************************************************************************************************************
Getters/Setters
***********************************************************************************************************************************/
typedef struct HrnHostPub
{
    StringId id;                                                    // Host id
    const String *name;                                             // Host name
    const String *container;                                        // Container name
    const String *image;                                            // Container image
    const String *ip;                                               // IP address
    const String *user;                                             // Container user
    const String *dataPath;                                         // Data path
    const Storage *dataStorage;                                     // Data storage
    const String *logPath;                                          // Log path
    const String *brBin;                                            // pgBackRest binary
    bool pgStandby;                                                 // Pg Standby
    const String *pgPath;                                           // Pg path
    const String *pgDataPath;                                       // Pg data path
    const String *pgTsPath;                                         // Pg tablespace path
    const String *pgLogFile;                                        // Pg log path
    const Storage *pgStorage;                                       // Pg storage
    const String *repo1Path;                                        // Repo1 path
    Storage *repo1Storage;                                          // Repo1 storage
    const String *repo2Path;                                        // Repo2 path
    Storage *repo2Storage;                                          // Repo2 storage
    const String *spoolPath;                                        // Spool path
    bool isPg;                                                      // Is a pg host?
    bool isRepo;                                                    // Is a repo host?
    bool updateHosts;                                               // Is /etc/hosts file updated on this host?
} HrnHostPub;

// Container name
FN_INLINE_ALWAYS const String *
hrnHostContainer(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->container;
}

// Host data path
FN_INLINE_ALWAYS const String *
hrnHostDataPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->dataPath;
}

// Host data storage
FN_INLINE_ALWAYS const Storage *
hrnHostDataStorage(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->dataStorage;
}

// Container image
FN_INLINE_ALWAYS const String *
hrnHostImage(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->image;
}

// Host id
FN_INLINE_ALWAYS StringId
hrnHostId(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->id;
}

// Container IP address
FN_INLINE_ALWAYS const String *
hrnHostIp(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->ip;
}

// Host name
FN_INLINE_ALWAYS const String *
hrnHostName(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->name;
}

// Is a pg host?
FN_INLINE_ALWAYS bool
hrnHostIsPg(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->isPg;
}

// Is a repo host?
FN_INLINE_ALWAYS bool
hrnHostIsRepo(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->isRepo;
}

// Log path
FN_INLINE_ALWAYS const String *
hrnHostLogPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->logPath;
}

// pgBackRest binary
FN_INLINE_ALWAYS const String *
hrnHostBrBin(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->brBin;
}

// Pg bin path
const String *hrnHostPgBinPath(HrnHost *this);

// Pg client
PgClient *hrnHostPgClient(HrnHost *this);

// Pg path
FN_INLINE_ALWAYS const String *
hrnHostPgPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgPath;
}

// Pg log file
FN_INLINE_ALWAYS const String *
hrnHostPgLogFile(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgLogFile;
}

// Pg data path
FN_INLINE_ALWAYS const String *
hrnHostPgDataPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgDataPath;
}

// Pg standby?
FN_INLINE_ALWAYS bool
hrnHostPgStandby(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgStandby;
}

// Host data storage
FN_INLINE_ALWAYS const Storage *
hrnHostPgStorage(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgStorage;
}

// Pg tablespace path
FN_INLINE_ALWAYS const String *
hrnHostPgTsPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->pgTsPath;
}

// Repo1 path
FN_INLINE_ALWAYS const String *
hrnHostRepo1Path(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->repo1Path;
}

// Repo1 storage
FN_INLINE_ALWAYS const Storage *
hrnHostRepo1Storage(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->repo1Storage;
}

// Repo2 path
FN_INLINE_ALWAYS const String *
hrnHostRepo2Path(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->repo2Path;
}

// Repo2 storage
FN_INLINE_ALWAYS const Storage *
hrnHostRepo2Storage(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->repo2Storage;
}

// Spool path
FN_INLINE_ALWAYS const String *
hrnHostSpoolPath(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->spoolPath;
}

// Host name
FN_INLINE_ALWAYS bool
hrnHostUpdateHosts(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->updateHosts;
}

// Container user
FN_INLINE_ALWAYS const String *
hrnHostUser(const HrnHost *const this)
{
    return THIS_PUB(HrnHost)->user;
}

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Execute a command
typedef struct HrnHostExecParam
{
    VAR_PARAM_HEADER;
    const String *user;                                             // User to execute command
    int resultExpect;                                               // Expected result, if not 0
} HrnHostExecParam;

#define hrnHostExecP(this, command, ...)                                                                                           \
    hrnHostExec(this, command, (HrnHostExecParam){VAR_PARAM_INIT, __VA_ARGS__})

String *hrnHostExec(HrnHost *this, const String *command, HrnHostExecParam param);

// Execute pgbackrest
#define TEST_HOST_BR(this, command, ...)                                                                                           \
    do                                                                                                                             \
    {                                                                                                                              \
        const HrnHostExecBrParam param = {VAR_PARAM_INIT, __VA_ARGS__};                                                            \
                                                                                                                                   \
        TEST_RESULT_INFO_FMT(                                                                                                      \
            "%s: pgbackrest%s %s%s%s", strZ(hrnHostName(this)), param.option == NULL ? "" : zNewFmt(" %s", param.option), command, \
            param.param == NULL ? "" : zNewFmt(" %s", param.param),                                                                \
            param.resultExpect != 0 ? zNewFmt(" -- result %d", param.resultExpect) : "");                                          \
        hrnHostExecBrP(this, command , __VA_ARGS__);                                                                               \
    }                                                                                                                              \
    while (0);

typedef struct HrnHostExecBrParam
{
    VAR_PARAM_HEADER;
    const char *user;                                               // User to execute command
    const char *option;                                             // Options
    const char *param;                                              // Parameters
    int resultExpect;                                               // Expected result, if not 0
} HrnHostExecBrParam;

#define hrnHostExecBrP(this, command, ...)                                                                                         \
    hrnHostExecBr(this, command, (HrnHostExecBrParam){VAR_PARAM_INIT, __VA_ARGS__})

String *hrnHostExecBr(HrnHost *this, const char *command, HrnHostExecBrParam param);

// Create/start/stop/promote pg cluster
#define HRN_HOST_PG_CREATE(this)                                                                                                   \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: create pg cluster", strZ(hrnHostName(this)));                                                    \
        hrnHostPgCreate(this);                                                                                                     \
    }                                                                                                                              \
    while (0)

void hrnHostPgCreate(HrnHost *this);

#define HRN_HOST_PG_START(this)                                                                                                    \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: start pg cluster", strZ(hrnHostName(this)));                                                     \
        hrnHostPgStart(this);                                                                                                      \
    }                                                                                                                              \
    while (0)

void hrnHostPgStart(HrnHost *this);

#define HRN_HOST_PG_STOP(this)                                                                                                     \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: stop pg cluster", strZ(hrnHostName(this)));                                                      \
        hrnHostPgStop(this);                                                                                                       \
    }                                                                                                                              \
    while (0)

void hrnHostPgStop(HrnHost *this);

#define HRN_HOST_PG_PROMOTE(this)                                                                                                  \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: promote pg cluster", strZ(hrnHostName(this)));                                                   \
        hrnHostPgPromote(this);                                                                                                    \
    }                                                                                                                              \
    while (0)

void hrnHostPgPromote(HrnHost *this);

// Query
PackRead *hrnHostSql(HrnHost *this, const String *statement, const PgClientQueryResult resultType);

FN_INLINE_ALWAYS PackRead *
hrnHostSqlLog(HrnHost *const this, int line, const char *const statement, const PgClientQueryResult resultType)
{
    TEST_RESULT_INFO_LINE_FMT(line, "%s: %s", strZ(hrnHostName(this)), statement);
    return hrnHostSql(this, STR(statement), resultType);
}

#define HRN_HOST_SQL(this, statement, resultType)                                                                                  \
    hrnHostSqlLog(this, __LINE__, statement, resultType)

// Exec statement
void hrnHostSqlExec(HrnHost *this, const String *statement);

#define HRN_HOST_SQL_EXEC(this, statement)                                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: %s", strZ(hrnHostName(this)), statement);                                                        \
        hrnHostSqlExec(this, STR(statement));                                                                                      \
    } while (0)

// Query a single value
FN_INLINE_ALWAYS PackRead *
hrnHostSqlValue(HrnHost *const this, const char *const statement)
{
    return hrnHostSql(this, STR(statement), pgClientQueryResultColumn);
}

#define HRN_HOST_SQL_VALUE(this, statement)                                                                                        \
    HRN_HOST_SQL(this, statement, pgClientQueryResultColumn)

// Test a single value
void hrnHostSqlTest(HrnHost *this, const String *statement, const String *expected);

#define TEST_HOST_SQL_ONE_STR_Z(this, statement, expected)                                                                         \
    do                                                                                                                             \
    {                                                                                                                              \
        TEST_RESULT_INFO_FMT("%s: %s == '%s'", strZ(hrnHostName(this)), statement, expected);                                      \
        hrnTestResultBegin(#statement, true);                                                                                      \
        hrnHostSqlTest(this, STR(statement), STR(expected));                                                                       \
    }                                                                                                                              \
    while (0)

// Begin transaction
FN_INLINE_ALWAYS void
hrnHostSqlBegin(HrnHost *const this)
{
    hrnHostSqlExec(this, STRDEF("begin"));
}

// Commit transaction
FN_INLINE_ALWAYS void
hrnHostSqlCommit(HrnHost *const this)
{
    hrnHostSqlExec(this, STRDEF("commit"));
}

// Switch WAL segment
#define HRN_HOST_WAL_SWITCH(this)                                                                                                  \
    HRN_HOST_SQL_EXEC(this, zNewFmt("select pg_switch_%s()::text", strZ(pgWalName(hrnHostPgVersion()))))

/***********************************************************************************************************************************
Helper functions
***********************************************************************************************************************************/
// Cipher Type
CipherType hrnHostCipherType(void);

// Cipher Passphrase
const String *hrnHostCipherPass(void);

// Compress Type
CompressType hrnHostCompressType(void);

// Full/incr enabled
bool hrnHostFullIncr(void);

// Non version-specific testing enabled
bool hrnHostNonVersionSpecific(void);

// Pg version
unsigned int hrnHostPgVersion(void);

// Pg1 host
HrnHost *hrnHostPg1(void);

// Pg2 host
HrnHost *hrnHostPg2(void);

// Repo host
HrnHost *hrnHostRepo(void);

// Repo total
unsigned int hrnHostRepoTotal(void);

// Does the primary repo have versioning enabled?
bool hrnHostRepoVersioning(void);

// Get a host by name
HrnHost *hrnHostGet(StringId id);

// Run hosts and build configuration based on test matrix
void hrnHostBuild(int line, const HrnHostTestDefine *testMatrix, size_t testMatrixSize);

#define HRN_HOST_BUILD(testMatrix)                                                                                                 \
    hrnHostBuild(__LINE__, testMatrix, LENGTH_OF(testMatrix))

// Update configuration
typedef struct HrnHostConfigUpdateParam
{
    VAR_PARAM_HEADER;
    const Variant *archiveAsync;                                    // Update async archiving?
} HrnHostConfigUpdateParam;

#define hrnHostConfigUpdateP(...)                                                                                                  \
    hrnHostConfigUpdate((HrnHostConfigUpdateParam){VAR_PARAM_INIT, __VA_ARGS__})

void hrnHostConfigUpdate(const HrnHostConfigUpdateParam param);

/***********************************************************************************************************************************
Destructor
***********************************************************************************************************************************/
FN_INLINE_ALWAYS void
hrnHostFree(HrnHost *const this)
{
    objFree(this);
}

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_HRN_HOST_TYPE                                                                                                 \
    HrnHost *
#define FUNCTION_LOG_HRN_HOST_FORMAT(value, buffer, bufferSize)                                                                    \
    objNameToLog(value, "HrnHost", buffer, bufferSize)

#endif
