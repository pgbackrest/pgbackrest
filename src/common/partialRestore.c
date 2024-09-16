#include "build.auto.h"
#include "common/type/json.h"
#include "common/type/list.h"
#include "config/config.h"
#include "partialRestore.h"
#include "storage/helper.h"

typedef struct DataBase
{
    // It must be the first element since it is used for sorting
    Oid dbOid;
    List *tables;
} DataBase;

typedef struct Table
{
    Oid spcNode;
    Oid relNode;
} Table;

static int
tableComparator(const Table *const a, const Table *const b)
{
    if (a->spcNode != b->spcNode)
        return a->spcNode > b->spcNode ? 1 : -1;

    if (a->relNode == b->relNode)
        return 0;

    return a->relNode > b->relNode ? 1 : -1;
}

static List *
buildFilterList(JsonRead *const json)
{
    List *const result = lstNewP(sizeof(DataBase), .comparator = lstComparatorUInt);

    // read database array
    jsonReadArrayBegin(json);
    while (jsonReadTypeNextIgnoreComma(json) != jsonTypeArrayEnd)
    {
        DataBase dataBase = {0};
        // read database object
        jsonReadObjectBegin(json);
        while (jsonReadTypeNextIgnoreComma(json) != jsonTypeObjectEnd)
        {
            const String *const dbKey = jsonReadKey(json);
            if (strEqZ(dbKey, "dbOid"))
            {
                dataBase.dbOid = jsonReadUInt(json);
            }
            else if (strEqZ(dbKey, "tables"))
            {
                dataBase.tables = lstNewP(sizeof(Table), .comparator = (ListComparator *) tableComparator);
                // read table array
                jsonReadArrayBegin(json);
                while (jsonReadTypeNextIgnoreComma(json) != jsonTypeArrayEnd)
                {
                    Table table = {.spcNode = DEFAULTTABLESPACE_OID};
                    // read table object
                    jsonReadObjectBegin(json);
                    while (jsonReadTypeNextIgnoreComma(json) != jsonTypeObjectEnd)
                    {
                        const String *const tableKey = jsonReadKey(json);
                        if (strEqZ(tableKey, "tablespace"))
                        {
                            table.spcNode = jsonReadUInt(json);
                        }
                        else if (strEqZ(tableKey, "relfilenode"))
                        {
                            table.relNode = jsonReadUInt(json);
                        }
                        else
                        {
                            jsonReadSkip(json);
                        }
                    }
                    jsonReadObjectEnd(json);

                    if (table.relNode == 0)
                    {
                        THROW(FormatError, "relfilenode field of table is missing");
                    }

                    lstAdd(dataBase.tables, &table);
                }
                jsonReadArrayEnd(json);

                lstSort(dataBase.tables, sortOrderAsc);
            }
            else
            {
                jsonReadSkip(json);
            }
        }
        jsonReadObjectEnd(json);

        if (dataBase.dbOid == 0)
        {
            THROW(FormatError, "dbOid field of table is missing");
        }
        if (dataBase.tables == NULL)
        {
            THROW(FormatError, "tables field of table is missing");
        }

        lstAdd(result, &dataBase);
    }
    jsonReadArrayEnd(json);

    return lstSort(result, sortOrderAsc);
}

FN_EXTERN __attribute__((unused)) bool
isRelationNeeded(const Oid dbNode, const Oid spcNode, const Oid relNode)
{
    if (!cfgOptionTest(cfgOptFilter))
        return true;

    if (pgDbIsSystemId(dbNode) && pgDbIsSystemId(relNode))
        return true;

    static List *filterList = NULL;
    if (filterList == NULL)
    {
        const String *const filter_path = cfgOptionStrNull(cfgOptFilter);
        if (!strBeginsWith(filter_path, FSLASH_STR))
        {
            THROW(AssertError, "The path to the filter info file is not absolute");
        }

        const Storage *const local_storage = storageLocal();
        StorageRead *const storageRead = storageNewReadP(local_storage, filter_path);
        Buffer *const jsonFile = storageGetP(storageRead);
        JsonRead *const jsonRead = jsonReadNew(strNewBuf(jsonFile));

        filterList = buildFilterList(jsonRead);

        jsonReadFree(jsonRead);
        bufFree(jsonFile);
        storageReadFree(storageRead);
    }

    const DataBase *const db = lstFind(filterList, &dbNode);
    if (db == NULL)
        return false;

    return
        pgDbIsSystemId(relNode) ||
        lstExists(db->tables, &(Table){.spcNode = spcNode, .relNode = relNode});
}
