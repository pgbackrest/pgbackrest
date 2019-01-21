/***********************************************************************************************************************************
PostgreSQL 9.1 Interface
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "postgres/interface/v091.h"

/***********************************************************************************************************************************
Include PostgreSQL Types
***********************************************************************************************************************************/
#include "postgres/interface/v091.auto.c"

/***********************************************************************************************************************************
Is the control file for this version of PostgreSQL?
***********************************************************************************************************************************/
bool
pgInterfaceIs091(const Buffer *controlFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, controlFile);
    FUNCTION_TEST_END();

    ASSERT(controlFile != NULL);

    ControlFileData *controlData = (ControlFileData *)bufPtr(controlFile);

    FUNCTION_TEST_RETURN(
        BOOL, controlData->pg_control_version == PG_CONTROL_VERSION && controlData->catalog_version_no == CATALOG_VERSION_NO);
}

/***********************************************************************************************************************************
Get information from pg_control in a common format
***********************************************************************************************************************************/
PgControl
pgInterfaceControl091(const Buffer *controlFile)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);
    ASSERT(pgInterfaceIs091(controlFile));

    PgControl result = {0};
    ControlFileData *controlData = (ControlFileData *)bufPtr(controlFile);

    result.systemId = controlData->system_identifier;
    result.controlVersion = controlData->pg_control_version;
    result.catalogVersion = controlData->catalog_version_no;

    result.pageSize = controlData->blcksz;
    result.walSegmentSize = controlData->xlog_seg_size;

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

/***********************************************************************************************************************************
Create pg_control for testing
***********************************************************************************************************************************/
#ifdef DEBUG

void
pgInterfaceControlTest091(PgControl pgControl, Buffer *buffer)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(PG_CONTROL, pgControl);
    FUNCTION_TEST_END();

    ControlFileData *controlData = (ControlFileData *)bufPtr(buffer);

    controlData->system_identifier = pgControl.systemId;
    controlData->pg_control_version = pgControl.controlVersion == 0 ? PG_CONTROL_VERSION : pgControl.controlVersion;
    controlData->catalog_version_no = pgControl.catalogVersion == 0 ? CATALOG_VERSION_NO : pgControl.catalogVersion;

    controlData->blcksz = pgControl.pageSize;
    controlData->xlog_seg_size = pgControl.walSegmentSize;

    FUNCTION_TEST_RETURN_VOID();
}

#endif
