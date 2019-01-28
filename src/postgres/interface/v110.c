/***********************************************************************************************************************************
PostgreSQL 11 Interface
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/log.h"
#include "postgres/interface/v110.h"

/***********************************************************************************************************************************
Include PostgreSQL Types
***********************************************************************************************************************************/
#include "postgres/interface/v110.auto.c"

/***********************************************************************************************************************************
Is the control file for this version of PostgreSQL?
***********************************************************************************************************************************/
bool
pgInterfaceIs110(const Buffer *controlFile)
{
    FUNCTION_TEST_BEGIN();
        FUNCTION_TEST_PARAM(BUFFER, controlFile);
    FUNCTION_TEST_END();

    ASSERT(controlFile != NULL);

    ControlFileData *controlData = (ControlFileData *)bufPtr(controlFile);

    FUNCTION_TEST_RETURN(
        controlData->pg_control_version == PG_CONTROL_VERSION && controlData->catalog_version_no == CATALOG_VERSION_NO);
}

/***********************************************************************************************************************************
Get information from pg_control in a common format
***********************************************************************************************************************************/
PgControl
pgInterfaceControl110(const Buffer *controlFile)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(BUFFER, controlFile);
    FUNCTION_LOG_END();

    ASSERT(controlFile != NULL);
    ASSERT(pgInterfaceIs110(controlFile));

    PgControl result = {0};
    ControlFileData *controlData = (ControlFileData *)bufPtr(controlFile);

    result.systemId = controlData->system_identifier;
    result.controlVersion = controlData->pg_control_version;
    result.catalogVersion = controlData->catalog_version_no;

    result.pageSize = controlData->blcksz;
    result.walSegmentSize = controlData->xlog_seg_size;

    result.pageChecksum = controlData->data_checksum_version != 0;

    FUNCTION_LOG_RETURN(PG_CONTROL, result);
}

/***********************************************************************************************************************************
Create pg_control for testing
***********************************************************************************************************************************/
#ifdef DEBUG

void
pgInterfaceControlTest110(PgControl pgControl, Buffer *buffer)
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

    controlData->data_checksum_version = pgControl.pageChecksum;

    FUNCTION_TEST_RETURN_VOID();
}

#endif
