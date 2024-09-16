/***********************************************************************************************************************************
Test Run
***********************************************************************************************************************************/
#include "common/harnessConfig.h"
#include "common/harnessStorage.h"
#include "common/type/json.h"
#include "postgres/interface/static.vendor.h"
#include "storage/posix/storage.h"

static void
testRun(void)
{
    const String *const jsonstr = STRDEF(
        "["
        "  {"
        "    \"dbOid\": 20002,"
        "    \"tables\": [],"
        "    \"dbName\": \"db3\""
        "  },"
        "  {"
        "    \"dbName\": \"db2\","
        "    \"dbOid\": 20001,"
        "    \"tables\": ["
        "      {"
        "        \"tablespace\": 1700,"
        "        \"relfilenode\": 16386"
        "      },"
        "      {"
        "        \"relfilenode\": 11000,"
        "        \"tablefqn\": \"public.t3\","
        "        \"tablespace\": 1700"
        "      },"
        "      {"
        "        \"relfilenode\": 10000,"
        "        \"tablespace\": 1701"
        "      }"
        "    ]"
        "  },"
        "  {"
        "    \"dbOid\": 20000,"
        "    \"tables\": ["
        "      {"
        "        \"tablespace\": 1600,"
        "        \"relfilenode\": 16384"
        "      },"
        "      {"
        "        \"tablespace\": 1601,"
        "        \"relfilenode\": 16385"
        "      },"
        "      {"
        "        \"relfilenode\": 16388"
        "      },"
        "      {"
        "        \"tablespace\": 1600,"
        "        \"relfilenode\": 16386"
        "      }"
        "    ]"
        "  }"
        "]");

    if (testBegin("parse valid json"))
    {
        JsonRead *json = jsonReadNew(jsonstr);
        List *filterList = buildFilterList(json);
        TEST_RESULT_UINT(lstSize(filterList), 3, "database count");

        DataBase *db1 = lstGet(filterList, 0);
        DataBase *db2 = lstGet(filterList, 1);
        DataBase *db3 = lstGet(filterList, 2);

        TEST_RESULT_UINT(db1->dbOid, 20000, "dbOid of 1st database");
        TEST_RESULT_UINT(db2->dbOid, 20001, "dbOid of 2nd database");
        TEST_RESULT_UINT(db3->dbOid, 20002, "dbOid of 3rd database");

        TEST_RESULT_UINT(lstSize(db1->tables), 4, "dbOid of 1st database");
        TEST_RESULT_UINT(lstSize(db2->tables), 3, "dbOid of 2nd database");
        TEST_RESULT_UINT(lstSize(db3->tables), 0, "dbOid of 3rd database");

        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1600, .relNode = 16384}), lstGet(db1->tables, 0), sizeof(Table)), 0, "1st table of 1st DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1600, .relNode = 16386}), lstGet(db1->tables, 1), sizeof(Table)), 0, "2nd table of 1st DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1601, .relNode = 16385}), lstGet(db1->tables, 2), sizeof(Table)), 0, "3rd table of 1st DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1663, .relNode = 16388}), lstGet(db1->tables, 3), sizeof(Table)), 0, "4th table of 1st DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1700, .relNode = 11000}), lstGet(db2->tables, 0), sizeof(Table)), 0, "1st table of 2nd DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1700, .relNode = 16386}), lstGet(db2->tables, 1), sizeof(Table)), 0, "2nd table of 2nd DB");
        TEST_RESULT_INT(
            memcmp(&((Table){.spcNode = 1701, .relNode = 10000}), lstGet(db2->tables, 2), sizeof(Table)), 0, "3rd table of 2nd DB");

        Table *found = lstFind(db1->tables, &((Table){.spcNode = 1600, .relNode = 16384}));
        TEST_RESULT_INT(memcmp(&((Table){.spcNode = 1600, .relNode = 16384}), found, sizeof(Table)), 0, "test find");
    }

    if (testBegin("parse invalid json"))
    {
        TEST_TITLE("missing dbOid");
        JsonRead *json = jsonReadNew(STRDEF("[{}]"));
        TEST_ERROR(buildFilterList(json), FormatError, "dbOid field of table is missing");

        TEST_TITLE("missing tables");
        json = jsonReadNew(STRDEF("[{\"dbOid\": 10}]"));
        TEST_ERROR(buildFilterList(json), FormatError, "tables field of table is missing");

        TEST_TITLE("missing relfilenode");
        json = jsonReadNew(STRDEF("[{\"dbOid\": 10, \"tables\": [{}]}]"));
        TEST_ERROR(buildFilterList(json), FormatError, "relfilenode field of table is missing");
    }

    if (testBegin("The --filter option"))
    {
        StringList *const argListCommon = strLstNew();
        hrnCfgArgRawZ(argListCommon, cfgOptStanza, "test1");
        hrnCfgArgRawZ(argListCommon, cfgOptPgPath, TEST_PATH "/pg");

        TEST_TITLE("The --filter option is not set");
        HRN_CFG_LOAD(cfgCmdRestore, strLstDup(argListCommon));
        TEST_RESULT_BOOL(isRelationNeeded(16384, 1663, 16390), true, "always true without --filter");

        TEST_TITLE("The --filter option is invalid");
        StringList *const argListInv = strLstDup(argListCommon);
        hrnCfgArgRawZ(argListInv, cfgOptFilter, "recovery_filter.json");
        HRN_CFG_LOAD(cfgCmdRestore, argListInv);
        TEST_ERROR(isRelationNeeded(16384, 1663, 16390), AssertError, "The path to the filter info file is not absolute");

        TEST_TITLE("The --filter option is correct");
        const Storage *const storageTest = storagePosixNewP(TEST_PATH_STR, .write = true);
        HRN_STORAGE_PUT_Z(storageTest, "recovery_filter.json", strZ(jsonstr));

        StringList *const argList = strLstDup(argListCommon);
        hrnCfgArgRawZ(argList, cfgOptFilter, TEST_PATH "/recovery_filter.json");
        HRN_CFG_LOAD(cfgCmdRestore, argList);
        TEST_RESULT_BOOL(isRelationNeeded(5, 1663, 1259), true, "always true for system DB and system table");
        TEST_RESULT_BOOL(isRelationNeeded(20002, 1600, 1259), true, "system table from DB which exists in JSON");
        TEST_RESULT_BOOL(isRelationNeeded(20005, 1600, 16384), false, "user DB doesn't exist in JSON");
        TEST_RESULT_BOOL(isRelationNeeded(20000, 1600, 16384), true, "user table exists in JSON");
        TEST_RESULT_BOOL(isRelationNeeded(5, 1600,  16394), false, "user table from system DB doesn't exist in JSON");
        TEST_RESULT_BOOL(isRelationNeeded(20002, 1600,  16394), false, "user table from user DB doesn't exist in JSON");
    }
}
