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
InfoArchive *infoArchiveNew(String *fileName, const bool ignoreMissing);

const String *infoArchiveId(const InfoArchive *this);
void infoArchiveCheckPg(const InfoArchive *this, const uint pgVersion, uint64_t pgSystemId);

void infoArchiveFree(InfoArchive *this);

#endif
