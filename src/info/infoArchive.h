/***********************************************************************************************************************************
Info Archive Handler for pgbackrest information
***********************************************************************************************************************************/
#ifndef INFO_INFOARCHIVE_H
#define INFO_INFOARCHIVE_H

/***********************************************************************************************************************************
Info Archive object
***********************************************************************************************************************************/
typedef struct InfoArchive InfoArchive;

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
InfoArchive *infoArchiveNew(const String *fileName, bool ignoreMissing);
const String *infoArchiveId(const InfoArchive *this);
void infoArchiveCheckPg(const InfoArchive *this, unsigned int pgVersion, uint64_t pgSystemId);

void infoArchiveFree(InfoArchive *this);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_DEBUG_INFO_ARCHIVE_TYPE                                                                                           \
    InfoArchive *
#define FUNCTION_DEBUG_INFO_ARCHIVE_FORMAT(value, buffer, bufferSize)                                                              \
    objToLog(value, "InfoArchive", buffer, bufferSize)

#endif
