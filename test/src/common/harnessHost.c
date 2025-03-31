/***********************************************************************************************************************************
Host Harness
***********************************************************************************************************************************/
#include "build.auto.h"

#include "build/common/exec.h"
#include "common/compress/helper.h"
#include "common/crypto/common.h"
#include "common/error/retry.h"
#include "common/io/io.h"
#include "common/type/json.h"
#include "common/wait.h"
#include "config/config.h"
#include "postgres/interface.h"
#include "postgres/version.h"
#include "storage/azure/storage.intern.h"
#include "storage/gcs/storage.intern.h"
#include "storage/posix/storage.h"
#include "storage/s3/storage.intern.h"
#include "storage/sftp/storage.h"

#include "common/harnessDebug.h"
#include "common/harnessHost.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
struct HrnHost
{
    HrnHostPub pub;                                                 // Publicly accessible variables
    const String *pgBinPath;                                        // Pg bin path
    PgClient *pgClient;                                             // Pg client
};

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
// Azure constants
#define HRN_HOST_AZURE_ACCOUNT                                      "azaccount"
#define HRN_HOST_AZURE_KEY                                          "YXpLZXk="
#define HRN_HOST_AZURE_CONTAINER                                    "azcontainer"

// GCS constants
#define HRN_HOST_GCS_BUCKET                                         "gcsbucket"
#define HRN_HOST_GCS_KEY                                            "testkey"
#define HRN_HOST_GCS_KEY_TYPE                                       "token"
#define HRN_HOST_GCS_PORT                                           4443

// S3 constants
#define HRN_HOST_S3_ACCESS_KEY                                      "accessKey1"
#define HRN_HOST_S3_ACCESS_SECRET_KEY                               "verySecretKey1"
#define HRN_HOST_S3_BUCKET                                          "pgbackrest-dev"
#define HRN_HOST_S3_ENDPOINT                                        "s3.amazonaws.com"
#define HRN_HOST_S3_REGION                                          "us-east-1"

// SFTP constants
#define HRN_HOST_SFTP_HOSTKEY_HASH_TYPE                             "sha1"
#define HRN_HOST_SFTP_KEY_PATH                                      "test/certificate/ssh"
#define HRN_HOST_SFTP_KEY_PRIVATE                                   HRN_HOST_SFTP_KEY_PATH "/id_rsa"
#define HRN_HOST_SFTP_KEY_PUBLIC                                    HRN_HOST_SFTP_KEY_PATH "/id_rsa.pub"

// TLS constants
#define HRN_HOST_TLS_CERT_PATH                                      "test/certificate"
#define HRN_HOST_TLS_CLIENT_CERT                                    HRN_HOST_TLS_CERT_PATH "/pgbackrest-test-client.crt"
#define HRN_HOST_TLS_CLIENT_KEY                                     HRN_HOST_TLS_CERT_PATH "/pgbackrest-test-client.key"
#define HRN_HOST_TLS_SERVER_CA                                      HRN_HOST_TLS_CERT_PATH "/pgbackrest-test-ca.crt"
#define HRN_HOST_TLS_SERVER_CERT                                    HRN_HOST_TLS_CERT_PATH "/pgbackrest-test-server.crt"
#define HRN_HOST_TLS_SERVER_KEY                                     HRN_HOST_TLS_CERT_PATH "/pgbackrest-test-server.key"

// Cipher passphrase
#define HRN_CIPHER_PASSPHRASE                                       "x"

/***********************************************************************************************************************************
Local variables
***********************************************************************************************************************************/
static struct HrnHostLocal
{
    MemContext *memContext;                                         // Mem context for variables

    String *stanza;                                                 // Stanza
    unsigned int pgVersion;                                         // Pg version
    StringId repoHost;                                              // Host acting as repository
    StringId storage;                                               // Storage type
    CompressType compressType;                                      // Compress type
    CipherType cipherType;                                          // Cipher type
    const String *cipherPass;                                       // Cipher passphrase
    unsigned int repoTotal;                                         // Repository total
    unsigned int restoreTotal;                                      // Restore counter used to name spool path
    bool tls;                                                       // Use TLS instead of SSH?
    bool bundle;                                                    // Bundling enabled?
    bool blockIncr;                                                 // Block incremental enabled?
    bool archiveAsync;                                              // Async archiving enabled?
    bool fullIncr;                                                  // Full/incr enabled?
    bool nonVersionSpecific;                                        // Run non version-specific tests?
    bool versioning;                                                // Is versioning enabled in the repo storage?

    List *hostList;                                                 // List of hosts
} hrnHostLocal;

/**********************************************************************************************************************************/
HrnHost *
hrnHostNew(const StringId id, const String *const container, const String *const image, const HrnHostNewParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING_ID, id);
        FUNCTION_HARNESS_PARAM(STRING, container);
        FUNCTION_HARNESS_PARAM(STRING, image);
        FUNCTION_HARNESS_PARAM(STRING, param.user);
        FUNCTION_HARNESS_PARAM(STRING, param.option);
        FUNCTION_HARNESS_PARAM(BOOL, param.noUpdateHosts);
        FUNCTION_HARNESS_PARAM(STRING, param.dataPath);
        FUNCTION_HARNESS_PARAM(BOOL, param.pgStandby);
        FUNCTION_HARNESS_PARAM(BOOL, param.isPg);
        FUNCTION_HARNESS_PARAM(BOOL, param.isRepo);
    FUNCTION_HARNESS_END();

    OBJ_NEW_BEGIN(HrnHost, .childQty = MEM_CONTEXT_QTY_MAX)
    {
        *this = (HrnHost)
        {
            .pub =
            {
                .id = id,
                .name = strIdToStr(id),
                .container = strDup(container),
                .image = strDup(image),
                .user = param.user == NULL ? strNewZ("root") : strDup(param.user),
                .brBin = strNewZ("pgbackrest"),
                .pgStandby = param.pgStandby,
                .updateHosts = !param.noUpdateHosts,
                .isPg = param.isPg,
                .isRepo = param.isRepo,
            },
        };

        // Initializes paths and storage
        if (param.dataPath != NULL)
        {
            this->pub.dataPath = strDup(param.dataPath);
            this->pub.dataStorage = storagePosixNewP(hrnHostDataPath(this), .write = true);
            this->pub.logPath = strNewFmt("%s/log", strZ(hrnHostDataPath(this)));
            this->pub.pgPath = strNewFmt("%s/pg", strZ(hrnHostDataPath(this)));
            this->pub.pgDataPath = strNewFmt("%s/data", strZ(hrnHostPgPath(this)));
            this->pub.pgTsPath = strNewFmt("%s/ts", strZ(hrnHostPgPath(this)));
            this->pub.pgStorage = storagePosixNewP(hrnHostPgDataPath(this), .write = true);
            this->pub.pgLogFile = strNewFmt("%s/postgresql.log", strZ(hrnHostLogPath(this)));
            this->pub.repo1Path = strNewFmt("%s/repo1", strZ(hrnHostDataPath(this)));
            this->pub.repo2Path = strNewFmt("%s/repo2", strZ(hrnHostDataPath(this)));
            this->pub.spoolPath = strNewFmt("%s/spool/0000", strZ(hrnHostDataPath(this)));
        }

        MEM_CONTEXT_TEMP_BEGIN()
        {
            // Add host to list
            if (hrnHostGet(hrnHostId(this)) != NULL)
                THROW_FMT(AssertError, "host '%s' already exists", strZ(hrnHostName(this)));

            lstAdd(hrnHostLocal.hostList, &this);

            // Remove prior container with same name if it exists
            execOneP(strNewFmt("docker rm -f %s", strZ(hrnHostContainer(this))));

            // Run container
            String *const command = strCatFmt(
                strNew(), "docker run -itd -h %s --name=%s", strZ(hrnHostName(this)), strZ(hrnHostContainer(this)));

            if (hrnHostDataPath(this) != NULL)
            {
                Storage *const storageData = storagePosixNewP(STR(testPath()), .write = true);
                storagePathCreateP(storageData, hrnHostLogPath(this), .mode = 0700);

                strCatFmt(command, " -v '%s:%s'", strZ(hrnHostDataPath(this)), strZ(hrnHostDataPath(this)));
            }

            if (param.option != NULL)
                strCatFmt(command, " %s", strZ(param.option));

            if (param.entryPoint != NULL)
                strCatFmt(command, " --entrypoint=%s --user=%s", strZ(param.entryPoint), strZ(hrnHostUser(this)));

            strCatFmt(command, " %s", strZ(hrnHostImage(this)));

            if (param.param != NULL)
                strCatFmt(command, " %s", strZ(param.param));

            execOneP(command);

            // Get IP address
            const String *const ip = strTrim(
                execOneP(strNewFmt("docker inspect --format '{{ .NetworkSettings.IPAddress }}' %s", strZ(hrnHostContainer(this)))));

            MEM_CONTEXT_PRIOR_BEGIN()
            {
                this->pub.ip = strDup(ip);
            }
            MEM_CONTEXT_PRIOR_END();

            // Update host IP addresses
            for (unsigned int hostIdx = 0; hostIdx < lstSize(hrnHostLocal.hostList); hostIdx++)
            {
                HrnHost *const host = *(HrnHost **)lstGet(hrnHostLocal.hostList, hostIdx);

                // Skip if current host
                if (hrnHostId(this) != hrnHostId(host))
                {
                    // Add this IP to other hosts
                    if (hrnHostUpdateHosts(host))
                    {
                        hrnHostExecP(
                            host,
                            strNewFmt("echo \"%s\t%s\" >> /etc/hosts", strZ(hrnHostIp(this)), strZ(hrnHostName(this))),
                            .user = STRDEF("root"));
                    }

                    // Add other hosts IP to this host
                    if (hrnHostUpdateHosts(this))
                    {
                        hrnHostExecP(
                            this,
                            strNewFmt("echo \"%s\t%s\" >> /etc/hosts", strZ(hrnHostIp(host)), strZ(hrnHostName(host))),
                            .user = STRDEF("root"));
                    }
                }
            }
        }
        MEM_CONTEXT_TEMP_END();
    }
    OBJ_NEW_END();

    FUNCTION_HARNESS_RETURN(HRN_HOST, this);
}

/**********************************************************************************************************************************/
String *
hrnHostExec(HrnHost *const this, const String *const command, const HrnHostExecParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRING, command);
        FUNCTION_HARNESS_PARAM(STRING, param.user);
        FUNCTION_HARNESS_PARAM(INT, param.resultExpect);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        strCat(
            result,
            execOneP(
                command,
                .shell = strNewFmt(
                    "docker exec -u %s %s sh -c", param.user == NULL ? strZ(hrnHostUser(this)) : strZ(param.user),
                    strZ(hrnHostContainer(this))),
                .resultExpect = param.resultExpect));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
String *
hrnHostExecBr(HrnHost *const this, const char *const command, const HrnHostExecBrParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRINGZ, command);
        FUNCTION_HARNESS_PARAM(STRINGZ, param.user);
        FUNCTION_HARNESS_PARAM(STRINGZ, param.option);
        FUNCTION_HARNESS_PARAM(STRINGZ, param.param);
        FUNCTION_HARNESS_PARAM(INT, param.resultExpect);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(command != NULL);

    String *const result = strNew();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const user = param.user == NULL ? NULL : STR(param.user);
        String *const commandStr = strCatFmt(strNew(), "pgbackrest --stanza=" HRN_STANZA);

        if (param.option != NULL)
            strCatFmt(commandStr, " %s", param.option);

        strCatFmt(commandStr, " %s", command);

        // Set unique spool path
        if (strcmp(command, CFGCMD_RESTORE) == 0 && hrnHostLocal.archiveAsync)
        {
            this->pub.spoolPath = strNewFmt("%s/spool/%04u", strZ(hrnHostDataPath(this)), hrnHostLocal.restoreTotal++);
            hrnHostConfigUpdateP();
        }

        if (param.param != NULL)
            strCatFmt(commandStr, " %s", param.param);

        strCat(result, hrnHostExecP(this, commandStr, .user = user, .resultExpect = param.resultExpect));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(STRING, result);
}

/**********************************************************************************************************************************/
static void
hrnHostPgConf(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const config = strNew();

        // Connection options
        strCatZ(config, "listen_addresses = '*'\n");
        strCatFmt(config, "unix_socket_directories = '%s'\n", strZ(hrnHostPgPath(this)));

        // Archive options
        strCatZ(config, "\n");
        strCatZ(config, "archive_mode = on\n");
        strCatZ(config, "wal_level = hot_standby\n");
        strCatZ(config, "max_wal_senders = 3\n");
        strCatZ(config, "hot_standby = on\n");
        strCatZ(config, "archive_command = 'pgbackrest --stanza=" HRN_STANZA " archive-push \"%p\"'\n");

        // Log options
        strCatZ(config, "\n");
        strCatFmt(config, "log_directory = '%s'\n", strZ(hrnHostLogPath(this)));
        strCatFmt(config, "log_filename = '%s'\n", strZ(strBase(hrnHostPgLogFile(this))));
        strCatZ(config, "log_rotation_age = 0\n");
        strCatZ(config, "log_rotation_size = 0\n");
        strCatZ(config, "log_error_verbosity = verbose\n");

        // Force parallel mode on to make sure we are disabling it and there are no issues. This is important for testing that 9.6
        // works since pg_stop_backup() is marked parallel safe and will error if run in a worker.
        if (hrnHostPgVersion() >= PG_VERSION_96)
        {
            strCatZ(config, "\n");

            if (hrnHostPgVersion() >= PG_VERSION_16)
                strCatZ(config, "debug_parallel_query = on\n");
            else
                strCatZ(config, "force_parallel_mode = on\n");

            strCatZ(config, "max_parallel_workers_per_gather = 2\n");
        }

        // Write postgresql.conf
        storagePutP(
            storageNewWriteP(
                hrnHostDataStorage(this), strNewFmt("%s/postgresql.conf", strZ(hrnHostPgDataPath(this))), .modeFile = 0600),
            BUFSTR(config));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnHostPgCreate(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create data directory
        storagePathCreateP(hrnHostDataStorage(this), hrnHostPgDataPath(this), .mode = 0700);

        // Init database
        String *const command = strCatFmt(
            strNew(), "%s/initdb -k --pgdata='%s' --auth=trust", strZ(hrnHostPgBinPath(this)), strZ(hrnHostPgDataPath(this)));

        if (hrnHostPgVersion() >= PG_VERSION_11)
            strCatZ(command, " --wal-segsize=1");

        hrnHostExecP(this, command);

        // Add replication to pg_hba
        hrnHostExecP(
            this,
            strNewFmt(
                "echo 'host\treplication\treplicator\tpg2\t\t\ttrust' >> '%s/pg_hba.conf'", strZ(hrnHostPgDataPath(this))));

        // Start Pg
        hrnHostPgStart(this);

        // Add replication user
        hrnHostSqlExec(this, STRDEF("create user replicator replication"));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnHostPgStart(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create postgresql.conf
        hrnHostPgConf(this);

        // Start Pg
        const String *const command = strNewFmt(
            "%s/pg_ctl start -D '%s' -l '%s' -s -w", strZ(hrnHostPgBinPath(this)), strZ(hrnHostPgDataPath(this)),
            strZ(hrnHostPgLogFile(this)));

        hrnHostExecP(this, command);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnHostPgStop(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Free old client
        pgClientFree(this->pgClient);
        this->pgClient = NULL;

        // Stop pg
        const String *const command = strNewFmt(
            "%s/pg_ctl stop -D '%s' -l '%s' -s -w", strZ(hrnHostPgBinPath(this)), strZ(hrnHostPgDataPath(this)),
            strZ(hrnHostPgLogFile(this)));

        hrnHostExecP(this, command);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnHostPgPromote(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Promote pg
        const String *const command = strNewFmt(
            "%s/pg_ctl promote -D '%s' -s -w", strZ(hrnHostPgBinPath(this)), strZ(hrnHostPgDataPath(this)));

        hrnHostExecP(this, command);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
PackRead *
hrnHostSql(HrnHost *const this, const String *const statement, const PgClientQueryResult resultType)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRING, statement);
        FUNCTION_HARNESS_PARAM(STRING_ID, resultType);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));
    ASSERT(statement != NULL);

    PackRead *result = NULL;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *const wait = waitNew(10000);
        ErrorRetry *const errRetry = errRetryNew();
        bool done = false;

        do
        {
            TRY_BEGIN()
            {
                Pack *const pack = pgClientQuery(
                    hrnHostPgClient(this), statement, resultType == pgClientQueryResultNone ? pgClientQueryResultAny : resultType);

                if (pack != NULL && resultType != pgClientQueryResultNone)
                {
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        result = pckReadNew(pack);
                        pckMove(pack, objMemContext(result));
                    }
                    MEM_CONTEXT_PRIOR_END();
                }

                done = true;
            }
            CATCH_ANY()
            {
                errRetryAddP(errRetry);

                if (!waitMore(wait))
                    THROWP(errRetryType(errRetry), strZ(errRetryMessage(errRetry)));
            }
            TRY_END();
        }
        while (!done);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(PACK_READ, result);
}

/**********************************************************************************************************************************/
void
hrnHostSqlExec(HrnHost *const this, const String *const statement)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRING, statement);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));
    ASSERT(statement != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const statementTrim = strTrim(strDup(statement));

        if (strBeginsWithZ(statementTrim, "perform"))
            hrnHostSql(this, strNewFmt("do $$begin %s; end$$", strZ(statement)), pgClientQueryResultNone);
        else
            hrnHostSql(this, statement, pgClientQueryResultNone);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
void
hrnHostSqlTest(HrnHost *const this, const String *const statement, const String *const expected)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRING, statement);
        FUNCTION_HARNESS_PARAM(STRING, expected);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));
    ASSERT(statement != NULL);
    ASSERT(expected != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Wait *const wait = waitNew(10000);
        bool done = false;

        do
        {
            const String *const actual = pckReadStrP(hrnHostSqlValue(this, strZ(statement)));
            done = strEq(actual, expected);

            if (done || !waitMore(wait))
                hrnTestResultZ(strZ(actual), strZ(expected), harnessTestResultOperationEq);
        }
        while (!done);
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/***********************************************************************************************************************************
Host pgBackRest configuration
***********************************************************************************************************************************/
// Helper to configure TLS
static void
hrnHostConfigTls(HrnHost *const this, String *const config, const char *const host)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
        FUNCTION_HARNESS_PARAM(STRING, config);
        FUNCTION_HARNESS_PARAM(STRINGZ, host);
    FUNCTION_HARNESS_END();

    if (hrnHostLocal.tls)
    {
        strCatFmt(config, "%s-host-type=tls\n", host);
        strCatFmt(config, "%s-host-cert-file=%s/" HRN_HOST_TLS_CLIENT_CERT "\n", host, hrnPathRepo());
        strCatFmt(config, "%s-host-key-file=%s/" HRN_HOST_TLS_CLIENT_KEY "\n", host, hrnPathRepo());
    }

    FUNCTION_HARNESS_RETURN_VOID();
}

static void
hrnHostConfig(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);

    MEM_CONTEXT_TEMP_BEGIN()
    {
        String *const config = strNew();

        // General options
        strCatZ(config, "[global]\n");
        strCatZ(config, "beta=y\n");
        strCatZ(config, "buffer-size=128KiB\n");
        strCatZ(config, "job-retry=0\n");
        strCatZ(config, "archive-timeout=20\n");
        strCatZ(config, "protocol-timeout=60\n");
        strCatZ(config, "db-timeout=45\n");
        strCatZ(config, "start-fast=y\n");

        // Increase process-max for non-posix storage since there will be more latency
        if (hrnHostLocal.storage == STORAGE_POSIX_TYPE)
            strCatZ(config, "process-max=2\n");

        // Log options
        strCatZ(config, "\n");
        strCatFmt(config, "log-path=%s\n", strZ(hrnHostLogPath(this)));
        strCatZ(config, "log-level-console=warn\n");
        strCatZ(config, "log-level-file=detail\n");
        strCatZ(config, "log-subprocess=n\n");

        // Compress options
        strCatZ(config, "\n");
        strCatFmt(config, "compress-type=%s\n", strZ(compressTypeStr(hrnHostLocal.compressType)));
        strCatFmt(config, "compress-level=1\n");
        strCatFmt(config, "compress-level-network=1\n");

        if (hrnHostIsPg(this))
        {
            // Enable archive async
            if (hrnHostLocal.archiveAsync)
            {
                strCatZ(config, "\n");
                strCatZ(config, "archive-async=y\n");
                strCatFmt(config, "spool-path=%s\n", strZ(hrnHostSpoolPath(this)));
            }

            // Recovery options
            const char *const primary = hrnHostId(this) == HRN_HOST_PG1 ? HRN_HOST_PG2_Z : HRN_HOST_PG1_Z;

            strCatZ(config, "\n");
            strCatFmt(config, "recovery-option=primary_conninfo=host=%s port=5432 user=replicator\n", primary);
        }

        // Repo options
        if (hrnHostIsRepo(this))
        {
            objFree(this->pub.repo1Storage);
            this->pub.repo1Storage = NULL;
            objFree(this->pub.repo2Storage);
            this->pub.repo2Storage = NULL;

            strCatZ(config, "\n");
            strCatFmt(config, "repo1-type=%s\n", strZ(strIdToStr(hrnHostLocal.storage)));
            strCatFmt(config, "repo1-path=%s\n", strZ(hrnHostRepo1Path(this)));
            strCatZ(config, "repo1-retention-full=2\n");

            if (hrnHostLocal.cipherType != cipherTypeNone)
            {
                strCatFmt(config, "repo1-cipher-type=%s\n", strZ(strIdToStr(hrnHostLocal.cipherType)));
                strCatFmt(config, "repo1-cipher-pass=%s\n", strZ(hrnHostLocal.cipherPass));
            }

            if (hrnHostLocal.bundle)
            {
                strCatZ(config, "repo1-bundle=y\n");
                // Set bundle size/limit smaller for testing
                strCatZ(config, "repo1-bundle-size=1MiB\n");
                strCatZ(config, "repo1-bundle-limit=64KiB\n");
            }

            if (hrnHostLocal.blockIncr)
            {
                ASSERT(hrnHostLocal.bundle);
                strCatZ(config, "repo1-block=y\n");
            }

            if (hrnHostLocal.fullIncr)
                strCatZ(config, "backup-full-incr=y\n");

            switch (hrnHostLocal.storage)
            {
                case STORAGE_AZURE_TYPE:
                {
                    strCatZ(config, "repo1-azure-account=" HRN_HOST_AZURE_ACCOUNT "\n");
                    strCatZ(config, "repo1-azure-key=" HRN_HOST_AZURE_KEY "\n");
                    strCatZ(config, "repo1-azure-container=" HRN_HOST_AZURE_CONTAINER "\n");
                    strCatZ(config, "repo1-azure-host=" HRN_HOST_AZURE_Z "\n");
                    strCatZ(config, "repo1-azure-uri-style=path\n");
                    strCatZ(config, "repo1-storage-verify-tls=n\n");

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        HrnHost *const azure = hrnHostGet(HRN_HOST_AZURE);

                        this->pub.repo1Storage = storageAzureNew(
                            hrnHostRepo1Path(this), true, 0, NULL, STRDEF(HRN_HOST_AZURE_CONTAINER), STRDEF(HRN_HOST_AZURE_ACCOUNT),
                            storageAzureKeyTypeShared, STRDEF(HRN_HOST_AZURE_KEY), 4 * 1024 * 1024, NULL, hrnHostIp(azure),
                            storageAzureUriStylePath, 443, ioTimeoutMs(), false, NULL, NULL);
                    }
                    MEM_CONTEXT_OBJ_END();

                    break;
                }

                case STORAGE_GCS_TYPE:
                {
                    strCatZ(config, "repo1-gcs-bucket=" HRN_HOST_GCS_BUCKET "\n");
                    strCatZ(config, "repo1-gcs-key-type=" HRN_HOST_GCS_KEY_TYPE "\n");
                    strCatZ(config, "repo1-gcs-key=" HRN_HOST_GCS_KEY "\n");
                    strCatZ(config, "repo1-gcs-endpoint=" HRN_HOST_GCS_Z ":" STRINGIFY(HRN_HOST_GCS_PORT) "\n");
                    strCatZ(config, "repo1-storage-verify-tls=n\n");

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        HrnHost *const gcs = hrnHostGet(HRN_HOST_GCS);

                        this->pub.repo1Storage = storageGcsNew(
                            hrnHostRepo1Path(this), true, 0, NULL, STRDEF(HRN_HOST_GCS_BUCKET), storageGcsKeyTypeToken,
                            STRDEF(HRN_HOST_GCS_KEY), 4 * 1024 * 1024, NULL,
                            strNewFmt("%s:%d", strZ(hrnHostIp(gcs)), HRN_HOST_GCS_PORT), ioTimeoutMs(), false, NULL, NULL, NULL);
                    }
                    MEM_CONTEXT_OBJ_END();

                    break;
                }

                case STORAGE_S3_TYPE:
                {
                    strCatZ(config, "repo1-s3-key=" HRN_HOST_S3_ACCESS_KEY "\n");
                    strCatZ(config, "repo1-s3-key-secret=" HRN_HOST_S3_ACCESS_SECRET_KEY "\n");
                    strCatZ(config, "repo1-s3-bucket=" HRN_HOST_S3_BUCKET "\n");
                    strCatZ(config, "repo1-s3-endpoint=" HRN_HOST_S3_ENDPOINT "\n");
                    strCatZ(config, "repo1-s3-region=" HRN_HOST_S3_REGION "\n");
                    strCatZ(config, "repo1-storage-verify-tls=n\n");

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        HrnHost *const s3 = hrnHostGet(HRN_HOST_S3);

                        this->pub.repo1Storage = storageS3New(
                            hrnHostRepo1Path(this), true, 0, NULL, STRDEF(HRN_HOST_S3_BUCKET), STRDEF(HRN_HOST_S3_ENDPOINT),
                            storageS3UriStyleHost, STR(HRN_HOST_S3_REGION), storageS3KeyTypeShared, STRDEF(HRN_HOST_S3_ACCESS_KEY),
                            STRDEF(HRN_HOST_S3_ACCESS_SECRET_KEY), NULL, NULL, NULL, NULL, NULL, 5 * 1024 * 1024, NULL,
                            hrnHostIp(s3), 443, ioTimeoutMs(), false, NULL, NULL, NULL);
                    }
                    MEM_CONTEXT_OBJ_END();

                    break;
                }

                case STORAGE_SFTP_TYPE:
                {
                    const String *const keyPrivate = strNewFmt("%s/" HRN_HOST_SFTP_KEY_PRIVATE, hrnPathRepo());
                    const String *const keyPublic = strNewFmt("%s/" HRN_HOST_SFTP_KEY_PUBLIC, hrnPathRepo());

                    strCatZ(config, "repo1-sftp-host=" HRN_HOST_SFTP_Z "\n");
                    strCatZ(config, "repo1-sftp-host-key-hash-type=" HRN_HOST_SFTP_HOSTKEY_HASH_TYPE "\n");
                    strCatFmt(config, "repo1-sftp-host-user=%s\n", strZ(hrnHostUser(this)));
                    strCatFmt(config, "repo1-sftp-private-key-file=%s\n", strZ(keyPrivate));
                    strCatFmt(config, "repo1-sftp-public-key-file=%s\n", strZ(keyPublic));
                    strCatZ(config, "repo1-sftp-host-key-check-type=none\n");

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        HrnHost *const sftp = hrnHostGet(HRN_HOST_SFTP);

                        this->pub.repo1Storage = storageSftpNewP(
                            hrnHostRepo1Path(this), hrnHostIp(sftp), 22, hrnHostUser(this), ioTimeoutMs(), keyPrivate,
                            strIdFromZ(HRN_HOST_SFTP_HOSTKEY_HASH_TYPE), .write = true, .modeFile = STORAGE_MODE_FILE_DEFAULT,
                            .modePath = STORAGE_MODE_PATH_DEFAULT, .keyPub = keyPublic,
                            .hostKeyCheckType = SFTP_STRICT_HOSTKEY_CHECKING_NONE);
                    }
                    MEM_CONTEXT_OBJ_END();

                    break;
                }

                default:
                {
                    ASSERT(hrnHostLocal.storage == STORAGE_POSIX_TYPE);

                    MEM_CONTEXT_OBJ_BEGIN(this)
                    {
                        this->pub.repo1Storage = storagePosixNewP(hrnHostRepo1Path(this), .write = true);
                    }
                    MEM_CONTEXT_OBJ_END();

                    break;
                }
            }

            if (hrnHostLocal.repoTotal > 1)
            {
                strCatFmt(config, "repo2-path=%s\n", strZ(hrnHostRepo2Path(this)));
                strCatZ(config, "repo2-retention-full=2\n");

                MEM_CONTEXT_OBJ_BEGIN(this)
                {
                    this->pub.repo2Storage = storagePosixNewP(hrnHostRepo2Path(this), .write = true);
                }
                MEM_CONTEXT_OBJ_END();
            }
        }
        // Pg options
        else
        {
            const HrnHost *const repo = hrnHostRepo();

            strCatZ(config, "\n");
            strCatFmt(config, "repo1-host=%s\n", strZ(hrnHostName(repo)));
            strCatFmt(config, "repo1-host-user=%s\n", strZ(hrnHostUser(repo)));
            hrnHostConfigTls(this, config, "repo1");

            if (hrnHostLocal.repoTotal > 1)
            {
                strCatFmt(config, "repo2-host=%s\n", strZ(hrnHostName(repo)));
                strCatFmt(config, "repo2-host-user=%s\n", strZ(hrnHostUser(repo)));
                hrnHostConfigTls(this, config, "repo2");
            }
        }

        // Stanza options
        strCatZ(config, "\n[" HRN_STANZA "]\n");

        switch (hrnHostId(this))
        {
            case HRN_HOST_PG1:
            {
                strCatFmt(config, "pg1-path=%s\n", strZ(hrnHostPgDataPath(this)));
                strCatFmt(config, "pg1-socket-path=%s/pg1/pg\n", testPath());

                break;
            }

            case HRN_HOST_PG2:
            {
                strCatFmt(config, "pg1-path=%s\n", strZ(hrnHostPgDataPath(this)));
                strCatFmt(config, "pg1-socket-path=%s/pg2/pg\n", testPath());

                if (hrnHostLocal.repoHost == HRN_HOST_PG2)
                {
                    strCatFmt(config, "pg2-path=%s/pg1/pg/data\n", testPath());
                    strCatZ(config, "pg2-host=pg1\n");
                    strCatFmt(config, "pg2-host-user=%s\n", strZ(hrnHostUser(this)));
                    hrnHostConfigTls(this, config, "pg2");
                }

                break;
            }

            default:
            {
                ASSERT(hrnHostId(this) == HRN_HOST_REPO);

                strCatFmt(config, "pg1-path=%s/pg1/pg/data\n", testPath());
                strCatZ(config, "pg1-host=pg1\n");
                strCatFmt(config, "pg1-host-user=%s\n", strZ(hrnHostUser(this)));
                hrnHostConfigTls(this, config, "pg1");

                strCatFmt(config, "pg2-path=%s/pg2/pg/data\n", testPath());
                strCatZ(config, "pg2-host=pg2\n");
                strCatFmt(config, "pg2-host-user=%s\n", strZ(hrnHostUser(this)));
                hrnHostConfigTls(this, config, "pg2");

                break;
            }
        }

        storagePutP(
            storageNewWriteP(hrnHostDataStorage(this), STRDEF("cfg/pgbackrest.conf"), .modeFile = 0600), BUFSTR(config));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
const String *
hrnHostPgBinPath(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    if (this->pgBinPath == NULL)
    {
        MEM_CONTEXT_TEMP_BEGIN()
        {
            const String *pgPath = strNewFmt("/usr/lib/postgresql/%s/bin", strZ(pgVersionToStr(hrnHostPgVersion())));

            TRY_BEGIN()
            {
                hrnHostExecP(this, strNewFmt("ls %s/initdb", strZ(pgPath)));
            }
            CATCH_ANY()
            {
                pgPath = strNewFmt("/usr/pgsql-%s/bin", strZ(pgVersionToStr(hrnHostPgVersion())));
            }
            TRY_END();

            MEM_CONTEXT_OBJ_BEGIN(this)
            {
                this->pgBinPath = strDup(pgPath);
            }
            MEM_CONTEXT_OBJ_END();
        }
        MEM_CONTEXT_TEMP_END();
    }

    FUNCTION_HARNESS_RETURN(STRING, this->pgBinPath);
}

/**********************************************************************************************************************************/
PgClient *
hrnHostPgClient(HrnHost *const this)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(HRN_HOST, this);
    FUNCTION_HARNESS_END();

    ASSERT(this != NULL);
    ASSERT(hrnHostIsPg(this));

    if (this->pgClient == NULL)
    {
        MEM_CONTEXT_OBJ_BEGIN(this)
        {
            this->pgClient = pgClientNew(hrnHostPgPath(this), 5432, STRDEF("postgres"), hrnHostUser(this), ioTimeoutMs());
            pgClientOpen(this->pgClient);
        }
        MEM_CONTEXT_OBJ_END();
    }

    FUNCTION_HARNESS_RETURN(PG_CLIENT, this->pgClient);
}

/**********************************************************************************************************************************/
HrnHost *
hrnHostGet(const StringId id)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(STRING_ID, id);
    FUNCTION_HARNESS_END();

    ASSERT(id != 0);

    HrnHost *result = NULL;

    for (unsigned int hostIdx = 0; hostIdx < lstSize(hrnHostLocal.hostList); hostIdx++)
    {
        HrnHost *const host = *(HrnHost **)lstGet(hrnHostLocal.hostList, hostIdx);

        if (hrnHostId(host) == id)
        {
            result = host;
            break;
        }
    }

    FUNCTION_HARNESS_RETURN(HRN_HOST, result);
}

/**********************************************************************************************************************************/
CipherType
hrnHostCipherType(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(STRING_ID, hrnHostLocal.cipherType);
}

const String *
hrnHostCipherPass(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(UINT, hrnHostLocal.cipherPass);
}

CompressType
hrnHostCompressType(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(ENUM, hrnHostLocal.compressType);
}

bool
hrnHostFullIncr(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(BOOL, hrnHostLocal.fullIncr);
}

bool
hrnHostNonVersionSpecific(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(BOOL, hrnHostLocal.nonVersionSpecific);
}

HrnHost *
hrnHostPg1(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(HRN_HOST, hrnHostGet(HRN_HOST_PG1));
}

HrnHost *
hrnHostPg2(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(HRN_HOST, hrnHostGet(HRN_HOST_PG2));
}

unsigned int
hrnHostPgVersion(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(UINT, hrnHostLocal.pgVersion);
}

HrnHost *
hrnHostRepo(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(HRN_HOST, hrnHostLocal.repoHost == HRN_HOST_PG2 ? hrnHostPg2() : hrnHostGet(HRN_HOST_REPO));
}

unsigned int
hrnHostRepoTotal(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(UINT, hrnHostLocal.repoTotal);
}

bool
hrnHostRepoVersioning(void)
{
    FUNCTION_HARNESS_VOID();
    FUNCTION_HARNESS_RETURN(UINT, hrnHostLocal.versioning);
}

/**********************************************************************************************************************************/
void
hrnHostConfigUpdate(const HrnHostConfigUpdateParam param)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(VARIANT, param.archiveAsync);
    FUNCTION_HARNESS_END();

    // Update config
    if (param.archiveAsync != NULL)
        hrnHostLocal.archiveAsync = varBool(param.archiveAsync);

    // Write pgBackRest configuration for hosts
    hrnHostConfig(hrnHostPg1());
    hrnHostConfig(hrnHostPg2());

    if (hrnHostLocal.repoHost == HRN_HOST_REPO)
        hrnHostConfig(hrnHostRepo());

    FUNCTION_HARNESS_RETURN_VOID();
}

/**********************************************************************************************************************************/
// Helper to run a host
static HrnHost *
hrnHostBuildRun(const int line, const StringId id)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, line);
        FUNCTION_HARNESS_PARAM(STRING_ID, id);
    FUNCTION_HARNESS_END();

    HrnHost *result;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        const String *const name = strIdToStr(id);
        const bool isPg = strBeginsWithZ(name, "pg");
        const bool isRepo = id == hrnHostLocal.repoHost;
        const String *const container = strNewFmt("test-%u-%s", testIdx(), strZ(name));
        const String *const image = strNewFmt("pgbackrest/test:%s-test-x86_64", testVm());
        const String *const dataPath = strNewFmt("%s/%s", testPath(), strZ(name));
        String *const option = strNewFmt(
            "-v '%s/cfg:/etc/pgbackrest:ro' -v '%s:/usr/bin/pgbackrest:ro' -v '%s:%s:ro'", strZ(dataPath), testProjectExe(),
            hrnPathRepo(), hrnPathRepo());
        const String *param = NULL;
        const String *entryPoint = NULL;

        if (hrnHostLocal.tls)
        {
            THROW_ON_SYS_ERROR_FMT(
                chmod(zNewFmt("%s/" HRN_HOST_TLS_SERVER_KEY, hrnPathRepo()), 0600) == -1, FileModeError,
                "unable to set mode on '%s/" HRN_HOST_TLS_SERVER_KEY "'", hrnPathRepo());
            THROW_ON_SYS_ERROR_FMT(
                chmod(zNewFmt("%s/" HRN_HOST_TLS_CLIENT_KEY, hrnPathRepo()), 0600) == -1, FileModeError,
                "unable to set mode on '%s/" HRN_HOST_TLS_CLIENT_KEY "'", hrnPathRepo());

            param = strNewFmt(
                "server --log-level-console=info --tls-server-ca-file=%s/" HRN_HOST_TLS_SERVER_CA " --tls-server-cert-file=%s/"
                HRN_HOST_TLS_SERVER_CERT " --tls-server-key-file=%s/" HRN_HOST_TLS_SERVER_KEY
                " --tls-server-auth=pgbackrest-client=* --tls-server-address=0.0.0.0", hrnPathRepo(), hrnPathRepo(), hrnPathRepo());
            entryPoint = strNewZ("/usr/bin/pgbackrest");
        }

        MEM_CONTEXT_PRIOR_BEGIN()
        {
            Storage *const storageData = storagePosixNewP(dataPath, .write = true);
            storagePutP(storageNewWriteP(storageData, STRDEF("cfg/pgbackrest.conf"), .modeFile = 0600), BUFSTRDEF(""));

            result = hrnHostNewP(
                id, container, image, .user = STR(testUser()), .dataPath = dataPath, .option = option, .param = param,
                .entryPoint = entryPoint, .isPg = isPg, .isRepo = isRepo);
        }
        MEM_CONTEXT_PRIOR_END();

        TEST_RESULT_INFO_LINE_FMT(line, "host %s (ip %s, container %s)", strZ(name), strZ(hrnHostIp(result)), strZ(container));

        if (isPg)
        {
            TEST_RESULT_INFO_LINE_FMT(
                line, "  psql: docker exec -it %s psql -U %s -h %s postgres", strZ(container), strZ(hrnHostUser(result)),
                strZ(hrnHostPgPath(result)));
        }
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_HARNESS_RETURN(HRN_HOST, result);
}

void
hrnHostBuild(const int line, const HrnHostTestDefine *const testMatrix, const size_t testMatrixSize)
{
    FUNCTION_HARNESS_BEGIN();
        FUNCTION_HARNESS_PARAM(INT, line);
        FUNCTION_HARNESS_PARAM_P(VOID, testMatrix);
        FUNCTION_HARNESS_PARAM(SIZE, testMatrixSize);
    FUNCTION_HARNESS_END();

    // Find test using pg version
    const HrnHostTestDefine *testDef = NULL;

    for (unsigned int matrixIdx = 0; matrixIdx < testMatrixSize; matrixIdx++)
    {
        if (strcmp(testMatrix[matrixIdx].pg, testPgVersion()) == 0)
        {
            testDef = &testMatrix[matrixIdx];
            break;
        }
    }

    if (testDef == NULL)
        THROW_FMT(AssertError, "unable to find PostgreSQL %s in test matrix", testPgVersion());

    // Initialize mem context
    if (hrnHostLocal.memContext == NULL)
    {
        MEM_CONTEXT_BEGIN(memContextTop())
        {
            MEM_CONTEXT_NEW_BEGIN(HrnHostLocal, .childQty = MEM_CONTEXT_QTY_MAX)
            {
                hrnHostLocal.memContext = MEM_CONTEXT_NEW();
                hrnHostLocal.hostList = lstNewP(sizeof(HrnHost *), .comparator = lstComparatorStr);
            }
            MEM_CONTEXT_NEW_END();
        }
        MEM_CONTEXT_END();
    }

    // Set local variables
    MEM_CONTEXT_BEGIN(hrnHostLocal.memContext)
    {
        hrnHostLocal.pgVersion = pgVersionFromStr(STR(testDef->pg));
        hrnHostLocal.repoHost = strIdFromZ(testDef->repo);
        hrnHostLocal.storage = strIdFromZ(testDef->stg);
        hrnHostLocal.compressType = compressTypeEnum(strIdFromZ(testDef->cmp));
        hrnHostLocal.cipherType = testDef->enc ? cipherTypeAes256Cbc : cipherTypeNone;
        hrnHostLocal.cipherPass = testDef->enc ? strNewZ(HRN_CIPHER_PASSPHRASE) : NULL;
        hrnHostLocal.repoTotal = testDef->rt;
        hrnHostLocal.tls = testDef->tls;
        hrnHostLocal.bundle = testDef->bnd;
        hrnHostLocal.blockIncr = testDef->bi;
        hrnHostLocal.fullIncr = testDef->fi;
        hrnHostLocal.nonVersionSpecific = strcmp(testDef->pg, testMatrix[testMatrixSize - 1].pg) == 0;
    }
    MEM_CONTEXT_END();

    ASSERT(hrnHostLocal.repoTotal >= 1 && hrnHostLocal.repoTotal <= 2);
    ASSERT(hrnHostLocal.repoHost == HRN_HOST_PG2 || hrnHostLocal.repoHost == HRN_HOST_REPO);

    TEST_RESULT_INFO_LINE_FMT(
        line, "pg = %s, repo = %s, .tls = %d, stg = %s, enc = %d, cmp = %s, rt = %u, bnd = %d, bi = %d, fi %d, nv = %d",
        testDef->pg, testDef->repo, testDef->tls, testDef->stg, testDef->enc, testDef->cmp, testDef->rt, testDef->bnd, testDef->bi,
        testDef->fi, hrnHostLocal.nonVersionSpecific);

    // Create pg hosts
    hrnHostBuildRun(line, HRN_HOST_PG1);
    HrnHost *const pg2 = hrnHostBuildRun(line, HRN_HOST_PG2);

    // Create repo host if the destination is repo
    HrnHost *const repo = hrnHostLocal.repoHost == HRN_HOST_REPO ? hrnHostBuildRun(line, HRN_HOST_REPO) : pg2;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        // Create storage host if not posix
        if (hrnHostLocal.storage != STORAGE_POSIX_TYPE)
        {
            const char *const fakeCertPath = zNewFmt("%s/doc/resource/fake-cert", hrnPathRepo());
            const String *const containerName = strNewFmt("test-%u-%s", testIdx(), strZ(strIdToStr(hrnHostLocal.storage)));

            switch (hrnHostLocal.storage)
            {
                case STORAGE_AZURE_TYPE:
                {
                    String *const option = strNewFmt(
                        "-v '%s/s3-server.crt:/root/public.crt:ro' -v '%s/s3-server.key:/root/private.key:ro'"
                        " -e AZURITE_ACCOUNTS='" HRN_HOST_AZURE_ACCOUNT ":" HRN_HOST_AZURE_KEY "'",
                        fakeCertPath, fakeCertPath);
                    String *const param = strNewZ(
                        "azurite-blob --blobPort 443 --blobHost 0.0.0.0 --cert=/root/public.crt --key=/root/private.key -d"
                        " debug.log --disableProductStyleUrl");

                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        hrnHostNewP(
                            HRN_HOST_AZURE, containerName, STRDEF("mcr.microsoft.com/azure-storage/azurite"), .option = option,
                            .param = param, .noUpdateHosts = true);
                    }
                    MEM_CONTEXT_PRIOR_END();

                    break;
                }

                case STORAGE_GCS_TYPE:
                {
                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        hrnHostNewP(HRN_HOST_GCS, containerName, STRDEF("fsouza/fake-gcs-server"), .noUpdateHosts = true);
                    }
                    MEM_CONTEXT_PRIOR_END();

                    break;
                }

                case STORAGE_S3_TYPE:
                {
                    String *const option = strNewFmt(
                        "-v '%s/s3-server.crt:/root/.minio/certs/public.crt:ro'"
                        " -v '%s/s3-server.key:/root/.minio/certs/private.key:ro' -e MINIO_REGION=" HRN_HOST_S3_REGION
                        " -e MINIO_DOMAIN=" HRN_HOST_S3_ENDPOINT " -e MINIO_BROWSER=off"
                        " -e MINIO_ACCESS_KEY=" HRN_HOST_S3_ACCESS_KEY " -e MINIO_SECRET_KEY=" HRN_HOST_S3_ACCESS_SECRET_KEY,
                        fakeCertPath, fakeCertPath);
                    String *const param = strNewZ("server /data --address :443");

                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        hrnHostNewP(
                            HRN_HOST_S3, containerName, STRDEF("minio/minio:RELEASE.2024-07-15T19-02-30Z"), .option = option,
                            .param = param, .noUpdateHosts = true);
                    }
                    MEM_CONTEXT_PRIOR_END();

                    HrnHost *const s3 = hrnHostGet(HRN_HOST_S3);

                    hrnHostExecP(
                        repo, strNewFmt("echo \"%s\t" HRN_HOST_S3_ENDPOINT "\" >> /etc/hosts", strZ(hrnHostIp(s3))),
                        .user = STRDEF("root"));
                    hrnHostExecP(
                        repo,
                        strNewFmt(
                            "echo \"%s\t" HRN_HOST_S3_BUCKET "." HRN_HOST_S3_ENDPOINT "\" >> /etc/hosts", strZ(hrnHostIp(s3))),
                        .user = STRDEF("root"));

                    break;
                }

                default:
                {
                    ASSERT(hrnHostLocal.storage == STORAGE_SFTP_TYPE);

                    MEM_CONTEXT_PRIOR_BEGIN()
                    {
                        hrnHostNewP(
                            HRN_HOST_SFTP, containerName, strNewFmt("pgbackrest/test:%s-test-x86_64", testVm()),
                            .noUpdateHosts = true);
                    }
                    MEM_CONTEXT_PRIOR_END();

                    break;
                }
            }
        }
    }
    MEM_CONTEXT_TEMP_END();

    // Write pgBackRest configuration for hosts
    hrnHostConfigUpdateP();

    // Create the bucket/container for object stores
    switch (hrnHostLocal.storage)
    {
        case STORAGE_AZURE_TYPE:
        {
            storageAzureRequestP(
                (StorageAzure *)storageDriver(hrnHostRepo1Storage(repo)), HTTP_VERB_PUT_STR,
                .query = httpQueryAdd(httpQueryNewP(), AZURE_QUERY_RESTYPE_STR, AZURE_QUERY_VALUE_CONTAINER_STR));

            break;
        }

        case STORAGE_GCS_TYPE:
        {
            JsonWrite *const json = jsonWriteObjectBegin(jsonWriteNewP());
            jsonWriteStr(jsonWriteKeyZ(json, GCS_JSON_NAME), STRDEF(HRN_HOST_GCS_BUCKET));
            jsonWriteObjectEnd(json);

            storageGcsRequestP(
                (StorageGcs *)storageDriver(hrnHostRepo1Storage(repo)), HTTP_VERB_POST_STR, .noBucket = true,
                .content = BUFSTR(jsonWriteResult(json)));

            break;
        }

        case STORAGE_S3_TYPE:
        {
            storageS3RequestP((StorageS3 *)storageDriver(hrnHostRepo1Storage(repo)), HTTP_VERB_PUT_STR, FSLASH_STR);
            storageS3RequestP(
                (StorageS3 *)storageDriver(hrnHostRepo1Storage(repo)), HTTP_VERB_PUT_STR, FSLASH_STR,
                .query = httpQueryAdd(httpQueryNewP(), STRDEF("versioning"), STRDEF("")),
                .content = BUFSTRDEF("<VersioningConfiguration><Status>Enabled</Status></VersioningConfiguration>"));

            hrnHostLocal.versioning = true;
            break;
        }
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
