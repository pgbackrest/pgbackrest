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

    (void)bldPg; // !!!

    // PostgreSQL interfaces
    // -----------------------------------------------------------------------------------------------------------------------------
    for (unsigned int pgIdx = 0; pgIdx < lstSize(bldPg.pgList); pgIdx++)
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
            "\n"
            "#define CheckPoint                                                  CheckPoint_%s\n"
            "#define ControlFileData                                             ControlFileData_%s\n"
            "#define DBState                                                     DBState_%s\n"
            "#define DB_STARTUP                                                  DB_STARTUP_%s\n"
            "#define DB_SHUTDOWNED                                               DB_SHUTDOWNED_%s\n"
            "#define DB_SHUTDOWNED_IN_RECOVERY                                   DB_SHUTDOWNED_IN_RECOVERY_%s\n"
            "#define DB_SHUTDOWNING                                              DB_SHUTDOWNING_%s\n"
            "#define DB_IN_CRASH_RECOVERY                                        DB_IN_CRASH_RECOVERY_%s\n"
            "#define DB_IN_ARCHIVE_RECOVERY                                      DB_IN_ARCHIVE_RECOVERY_%s\n"
            "#define DB_IN_PRODUCTION                                            DB_IN_PRODUCTION_%s\n"
            "#define FullTransactionId                                           FullTransactionId_%s\n"
            "#define XLogLongPageHeaderData                                      XLogLongPageHeaderData_%s\n"
            "#define XLogPageHeaderData                                          XLogPageHeaderData_%s\n"
            "#define XLogRecPtr                                                  XLogRecPtr_%s\n"
            "\n",
            strZ(pgVersion->version), versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot,
            versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot, versionNoDot,
            versionNoDot);

        if (!pgVersion->release)
        {
            strCatZ(
                pg,
                "#define CATALOG_VERSION_NO_MAX\n"
                "\n");
        }

        strCatFmt(
            pg,
            "#include \"postgres/interface/version.intern.h\"\n"
            "\n"
            "PG_INTERFACE(%s);\n"
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
            "#undef PG_INTERFACE_WAL\n",
            versionNum);
    }

    bldPut(storageRepo, "postgres/interface.auto.c.inc", BUFSTR(pg));
}

/**********************************************************************************************************************************/
void
bldPgRender(const Storage *const storageRepo, const BldPg bldPg)
{
    bldPgRenderInterfaceAutoC(storageRepo, bldPg);
}
