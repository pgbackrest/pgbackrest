/***********************************************************************************************************************************
Test Server Command
***********************************************************************************************************************************/
#include "common/exit.h"
#include "storage/posix/storage.h"
#include "storage/remote/storage.h"

#include "common/harnessConfig.h"
#include "common/harnessFork.h"
#include "common/harnessServer.h"
#include "common/harnessStorage.h"

/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
void
testRun(void)
{
    FUNCTION_HARNESS_VOID();

    Storage *storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);

    harnessLogLevelSet(logLevelDetail);

    // *****************************************************************************************************************************
    if (testBegin("cmdServer()"))
    {
        TEST_TITLE("server");

        HRN_FORK_BEGIN(.timeout = 10000)
        {
            HRN_FORK_CHILD_BEGIN(.prefix = "client repo")
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptPgPath, "/BOGUS");
                hrnCfgArgRaw(argList, cfgOptRepoHost, hrnServerHost());
                hrnCfgArgRawZ(argList, cfgOptRepoHostConfig, TEST_PATH "/pgbackrest.conf");
                hrnCfgArgRawZ(argList, cfgOptRepoHostType, "tls");
#if !TEST_IN_CONTAINER
                hrnCfgArgRawZ(argList, cfgOptRepoHostCaFile, HRN_SERVER_CA);
#endif
                hrnCfgArgRawZ(argList, cfgOptRepoHostCertFile, HRN_SERVER_CLIENT_CERT);
                hrnCfgArgRawZ(argList, cfgOptRepoHostKeyFile, HRN_SERVER_CLIENT_KEY);
                hrnCfgArgRawFmt(argList, cfgOptRepoHostPort, "%u", hrnServerPort(0));
                hrnCfgArgRawZ(argList, cfgOptStanza, "db");
                HRN_CFG_LOAD(cfgCmdArchiveGet, argList);

                // Client 1
                const Storage *storageRemote = NULL;
                TEST_ASSIGN(
                    storageRemote,
                    storageRemoteNew(
                        STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL,
                        protocolRemoteGet(protocolStorageTypeRepo, 0), cfgOptionUInt(cfgOptCompressLevelNetwork)),
                    "new storage 1");

                HRN_STORAGE_PUT_Z(storageRemote, "client1.txt", "CLIENT1");

                TEST_RESULT_VOID(protocolRemoteFree(0), "free client 1");

                // Client 2
                TEST_ASSIGN(
                    storageRemote,
                    storageRemoteNew(
                        STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL,
                        protocolRemoteGet(protocolStorageTypeRepo, 0), cfgOptionUInt(cfgOptCompressLevelNetwork)),
                    "new storage 2");

                HRN_STORAGE_PUT_Z(storageRemote, "client2.txt", "CLIENT2");

                TEST_RESULT_VOID(protocolRemoteFree(0), "free client 2");

                // Notify parent on exit
                HRN_FORK_CHILD_NOTIFY_PUT();
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_CHILD_BEGIN(.prefix = "client pg")
            {
                StringList *argList = strLstNew();
                hrnCfgArgRawZ(argList, cfgOptRepoPath, "/BOGUS");
                hrnCfgArgRaw(argList, cfgOptPgHost, hrnServerHost());
                hrnCfgArgRawZ(argList, cfgOptPgPath, TEST_PATH "/pg");
                hrnCfgArgRawZ(argList, cfgOptPgHostType, "tls");
#if !TEST_IN_CONTAINER
                hrnCfgArgRawZ(argList, cfgOptPgHostCaFile, HRN_SERVER_CA);
#endif
                hrnCfgArgRawZ(argList, cfgOptPgHostCertFile, HRN_SERVER_CLIENT_CERT);
                hrnCfgArgRawZ(argList, cfgOptPgHostKeyFile, HRN_SERVER_CLIENT_KEY);
                hrnCfgArgRawFmt(argList, cfgOptPgHostPort, "%u", hrnServerPort(0));
                hrnCfgArgRawZ(argList, cfgOptStanza, "db");
                hrnCfgArgRawZ(argList, cfgOptProcess, "1");
                HRN_CFG_LOAD(cfgCmdBackup, argList, .role = cfgCmdRoleLocal);

                // Client 3
                const Storage *storageRemote = NULL;
                TEST_ASSIGN(
                    storageRemote,
                    storageRemoteNew(
                        STORAGE_MODE_FILE_DEFAULT, STORAGE_MODE_PATH_DEFAULT, true, NULL,
                        protocolRemoteGet(protocolStorageTypePg, 0), cfgOptionUInt(cfgOptCompressLevelNetwork)),
                    "new storage 3");

                HRN_STORAGE_PUT_Z(storageRemote, "client3.txt", "CLIENT3");

                TEST_RESULT_VOID(protocolRemoteFree(0), "free client 3");

                // Notify parent on exit
                HRN_FORK_CHILD_NOTIFY_PUT();
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "client control")
            {
                HRN_FORK_BEGIN(.timeout = 10000)
                {
                    HRN_FORK_CHILD_BEGIN(.prefix = "server")
                    {
                        StringList *argList = strLstNew();
                        hrnCfgArgRawZ(argList, cfgOptTlsServerCaFile, HRN_SERVER_CA);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerCertFile, HRN_SERVER_CERT);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerKeyFile, HRN_SERVER_KEY);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerAuth, "pgbackrest-client=db");
                        hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                        HRN_CFG_LOAD(cfgCmdServerStart, argList);

                        // Write a config file to demonstrate that settings are loaded
                        HRN_STORAGE_PUT_Z(storageTest, "pgbackrest.conf", "[global]\nrepo1-path=" TEST_PATH "/repo");

                        // Init exit signal handlers
                        exitInit();

                        cmdServerInit(); // REMOVE !!!

                        // No log testing needed
                        harnessLogLevelSet(logLevelWarn);

                        // Get pid of this process to identify child process later
                        pid_t pid = getpid();

                        TEST_RESULT_VOID(cmdServer(), "server");

                        // If this is a child process then exit immediately
                        if (pid != getpid())
                        {
                            HRN_FORK_CHILD_NOTIFY_PUT();
                            exit(0);
                        }
                    }
                    HRN_FORK_CHILD_END();

                    HRN_FORK_PARENT_BEGIN(.prefix = "server control")
                    {
                        // Wait for forked child processes to exit
                        HRN_FORK_PARENT_NOTIFY_GET(0);
                        HRN_FORK_PARENT_NOTIFY_GET(0);
                        HRN_FORK_PARENT_NOTIFY_GET(0);

                        // Send term to child processes
                        kill(HRN_FORK_PROCESS_ID(0), SIGTERM);
                    }
                    HRN_FORK_PARENT_END();
                }
                HRN_FORK_END();

                // Wait for both child processes to exit
                HRN_FORK_PARENT_NOTIFY_GET(0);
                HRN_FORK_PARENT_NOTIFY_GET(1);

                // Check that files written by child processes are present
                TEST_STORAGE_GET(storageTest, "repo/client1.txt", "CLIENT1");
                TEST_STORAGE_GET(storageTest, "repo/client2.txt", "CLIENT2");
                TEST_STORAGE_GET(storageTest, "pg/client3.txt", "CLIENT3");
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    // *****************************************************************************************************************************
    if (testBegin("cmdServerPing()"))
    {
        TEST_TITLE("error on extra parameters");

        StringList *argList = strLstNew();
        strLstAddZ(argList, "host");
        strLstAddZ(argList, "bogus");
        HRN_CFG_LOAD(cfgCmdServerPing, argList);

        TEST_ERROR(cmdServerPing(), ParamInvalidError, "extra parameters found");

        HRN_FORK_BEGIN(.timeout = 5000)
        {

            HRN_FORK_CHILD_BEGIN(.prefix = "client")
            {
                TEST_TITLE("ping localhost");

                argList = strLstNew();
                hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                HRN_CFG_LOAD(cfgCmdServerPing, argList);

                TEST_RESULT_VOID(cmdServerPing(), "ping");

                // -----------------------------------------------------------------------------------------------------------------
                TEST_TITLE("ping 12.0.0.1");

                argList = strLstNew();
                hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                strLstAddZ(argList, "127.0.0.1");
                HRN_CFG_LOAD(cfgCmdServerPing, argList);

                TEST_RESULT_VOID(cmdServerPing(), "ping");

                // Notify parent on exit
                HRN_FORK_CHILD_NOTIFY_PUT();
            }
            HRN_FORK_CHILD_END();

            HRN_FORK_PARENT_BEGIN(.prefix = "client control")
            {
                HRN_FORK_BEGIN(.timeout = 5000)
                {
                    HRN_FORK_CHILD_BEGIN(.prefix = "server")
                    {
                        StringList *argList = strLstNew();
                        hrnCfgArgRawZ(argList, cfgOptTlsServerCaFile, HRN_SERVER_CA);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerCertFile, HRN_SERVER_CERT);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerKeyFile, HRN_SERVER_KEY);
                        hrnCfgArgRawZ(argList, cfgOptTlsServerAuth, "bogus=*");
                        hrnCfgArgRawFmt(argList, cfgOptTlsServerPort, "%u", hrnServerPort(0));
                        HRN_CFG_LOAD(cfgCmdServerStart, argList);

                        // Init exit signal handlers
                        exitInit();

                        // No log testing needed
                        harnessLogLevelSet(logLevelWarn);

                        // Get pid of this process to identify child process later
                        pid_t pid = getpid();

                        TEST_RESULT_VOID(cmdServer(), "server");

                        // If this is a child process then exit immediately
                        if (pid != getpid())
                        {
                            HRN_FORK_CHILD_NOTIFY_PUT();
                            exit(0);
                        }
                    }
                    HRN_FORK_CHILD_END();

                    HRN_FORK_PARENT_BEGIN(.prefix = "server control")
                    {
                        // Wait for forked child processes to exit
                        HRN_FORK_PARENT_NOTIFY_GET(0);
                        HRN_FORK_PARENT_NOTIFY_GET(0);

                        // Send term to child processes
                        kill(HRN_FORK_PROCESS_ID(0), SIGTERM);
                    }
                    HRN_FORK_PARENT_END();
                }
                HRN_FORK_END();

                // Wait for child process to exit
                HRN_FORK_PARENT_NOTIFY_GET(0);
            }
            HRN_FORK_PARENT_END();
        }
        HRN_FORK_END();
    }

    FUNCTION_HARNESS_RETURN_VOID();
}
