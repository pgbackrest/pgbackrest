/***********************************************************************************************************************************
Build Command
***********************************************************************************************************************************/
#include "build.auto.h"

#include "command/build/build.h"
#include "command/build/man.h"
#include "command/build/pre.h"
#include "command/build/reference.h"
#include "common/debug.h"
#include "common/log.h"
#include "storage/posix/storage.h"
#include "version.h"

/**********************************************************************************************************************************/
void
cmdBuild(const String *const pathRepo)
{
    FUNCTION_LOG_BEGIN(logLevelDebug);
        FUNCTION_LOG_PARAM(STRING, pathRepo);
    FUNCTION_LOG_END();

    MEM_CONTEXT_TEMP_BEGIN()
    {
        Storage *const storageRepo = storagePosixNewP(pathRepo, .write = true);
        const BldCfg bldCfg = bldCfgParse(storageRepo);
        const BldHlp bldHlp = bldHlpParse(storageRepo, bldCfg, true);
        XmlNode *const index = xmlDocumentRoot(
            xmlDocumentNewBuf(storageGetP(storageNewReadP(storageRepo, STRDEF("doc/xml/index.xml")))));
        XmlDocument *const userGuide = xmlDocumentNewBuf(
            storageGetP(storageNewReadP(storageRepo, STRDEF("doc/xml/user-guide.xml"))));

        storagePutP(
            storageNewWriteP(storageRepo, STRDEF("doc/output/xml/command.xml")),
            xmlDocumentBuf(referenceCommandRender(&bldCfg, &bldHlp)));
        storagePutP(
            storageNewWriteP(storageRepo, STRDEF("doc/output/xml/configuration.xml")),
            xmlDocumentBuf(referenceConfigurationRender(&bldCfg, &bldHlp)));
        storagePutP(
            storageNewWriteP(storageRepo, STRDEF("doc/output/xml/user-guide.xml")),
            xmlDocumentBuf(buildPre(userGuide, &bldCfg, &bldHlp)));
        storagePutP(
            storageNewWriteP(storageRepo, STRDEF("doc/output/man/" PROJECT_BIN ".1.txt")),
            BUFSTR(referenceManRender(index, &bldCfg, &bldHlp)));
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN_VOID();
}
