/***********************************************************************************************************************************
Render PostgreSQL Interface
***********************************************************************************************************************************/
#include "build.auto.h"

#include "storage/posix/storage.h"

#include "build/common/render.h"
#include "build/postgres/render.h"

/***********************************************************************************************************************************
Render interface.auto.c.inc
***********************************************************************************************************************************/
#define PG_MODULE                                                  "postgres"
#define PG_AUTO_COMMENT                                            "PostgreSQL Interface"

static void
bldPgRenderInterfaceAutoC(const Storage *const storageRepo, const BldPg bldPg)
{
    String *pg = bldHeader(PG_MODULE, PG_AUTO_COMMENT);

    // PostgreSQL interfaces
    // -----------------------------------------------------------------------------------------------------------------------------
    for (unsigned int pgIdx = lstSize(bldPg.pgList) - 1; pgIdx < lstSize(bldPg.pgList); pgIdx--)
    {
        const BldPgVersion *const pgVersion = lstGet(bldPg.pgList, pgIdx);
        const char *const versionNoDot = strZ(strLstJoin(strLstNewSplitZ(pgVersion->version, "."), ""));
        const char *const versionNum = versionNoDot[0] == '9' ? zNewFmt("0%s", versionNoDot) : zNewFmt("%s0", versionNoDot);

        strCatFmt(
            pg,
            "\n"
            COMMENT_BLOCK_BEGIN "\n"
            "PostgreSQL %s interface\n"
            COMMENT_BLOCK_END "\n"
            "#define PG_VERSION                                                  PG_VERSION_%s\n"
            "\n",
            strZ(pgVersion->version), versionNoDot);

        for (unsigned int typeIdx = 0; typeIdx < strLstSize(bldPg.typeList); typeIdx++)
        {
            const String *const type = strLstGet(bldPg.typeList, typeIdx);

            strCat(pg, bldDefineRender(type, strNewFmt("%s_%s", strZ(type), versionNoDot)));
            strCatChr(pg, '\n');
        }

        if (!pgVersion->release)
        {
            strCatZ(
                pg,
                "\n"
                "#define CATALOG_VERSION_NO_MAX\n");
        }

        strCatZ(
            pg,
            "\n"
            "#include \"postgres/interface/version.intern.h\"\n"
            "\n");

        for (unsigned int functionIdx = 0; functionIdx < strLstSize(bldPg.functionList); functionIdx++)
            strCatFmt(pg, "%s(%s);\n", strZ(strLstGet(bldPg.functionList, functionIdx)), versionNum);

        strCatChr(pg, '\n');

        for (unsigned int typeIdx = 0; typeIdx < strLstSize(bldPg.typeList); typeIdx++)
            strCatFmt(pg, "#undef %s\n", strZ(strLstGet(bldPg.typeList, typeIdx)));

        strCatChr(pg, '\n');

        for (unsigned int defineIdx = 0; defineIdx < strLstSize(bldPg.defineList); defineIdx++)
            strCatFmt(pg, "#undef %s\n", strZ(strLstGet(bldPg.defineList, defineIdx)));

        strCatChr(pg, '\n');

        for (unsigned int functionIdx = 0; functionIdx < strLstSize(bldPg.functionList); functionIdx++)
            strCatFmt(pg, "#undef %s\n", strZ(strLstGet(bldPg.functionList, functionIdx)));
    }

    // Interface struct
    // -----------------------------------------------------------------------------------------------------------------------------
    strCatFmt(
        pg,
        "\n"
        COMMENT_BLOCK_BEGIN "\n"
        "PostgreSQL interface struct\n"
        COMMENT_BLOCK_END "\n"
        "static const PgInterface pgInterface[] =\n"
        "{\n");

    for (unsigned int pgIdx = lstSize(bldPg.pgList) - 1; pgIdx < lstSize(bldPg.pgList); pgIdx--)
    {
        const BldPgVersion *const pgVersion = lstGet(bldPg.pgList, pgIdx);
        const char *const versionNoDot = strZ(strLstJoin(strLstNewSplitZ(pgVersion->version, "."), ""));
        const char *const versionNum = versionNoDot[0] == '9' ? zNewFmt("0%s", versionNoDot) : zNewFmt("%s0", versionNoDot);

        strCatFmt(
            pg,
            "    {\n"
            "        .version = PG_VERSION_%s,\n"
            "\n"
            "        .controlIs = pgInterfaceControlIs%s,\n"
            "        .control = pgInterfaceControl%s,\n"
            "        .controlVersion = pgInterfaceControlVersion%s,\n"
            "\n"
            "        .walIs = pgInterfaceWalIs%s,\n"
            "        .wal = pgInterfaceWal%s,\n"
            "    },\n",
            versionNoDot, versionNum, versionNum, versionNum, versionNum, versionNum);
    }

    strCatFmt(
        pg,
        "};\n");

    bldPut(storageRepo, "postgres/interface.auto.c.inc", BUFSTR(pg));
}

/**********************************************************************************************************************************/
void
bldPgRender(const Storage *const storageRepo, const BldPg bldPg)
{
    bldPgRenderInterfaceAutoC(storageRepo, bldPg);
}
