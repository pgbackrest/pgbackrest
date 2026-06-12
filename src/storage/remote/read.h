/***********************************************************************************************************************************
Remote Storage Read
***********************************************************************************************************************************/
#ifndef STORAGE_REMOTE_READ_H
#define STORAGE_REMOTE_READ_H

#include "protocol/client.h"
#include "storage/read.h"
#include "storage/remote/storage.intern.h"

/***********************************************************************************************************************************
Object type
***********************************************************************************************************************************/
typedef struct StorageReadRemote StorageReadRemote;

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
FN_EXTERN StorageReadRemote *storageReadRemoteNew(
    StorageRemote *storage, ProtocolClient *client, const String *name, bool compressible, unsigned int compressLevel,
    uint64_t offset, const Variant *limit, const String *versionId);

/***********************************************************************************************************************************
Stream state and functions shared with the remote multi read
***********************************************************************************************************************************/
typedef struct StorageReadRemoteStream
{
    MemContext *memContext;                                         // Object context used to allocate chunks
    ProtocolClientSession *session;                                 // Protocol session for requests
    size_t remaining;                                               // Bytes remaining to be read in chunk
    Buffer *chunk;                                                  // Chunk currently being read
    bool eof;                                                       // Has the read reached eof?
    bool eofFound;                                                  // Eof found but a chunk is remaining to be read

#ifdef DEBUG
    uint64_t protocolReadBytes;                                     // How many bytes were read from the protocol layer?
#endif
} StorageReadRemoteStream;

// Process a response pack containing chunk data and/or eof with filter results
FN_EXTERN void storageReadRemoteStreamInternal(StorageReadRemoteStream *this, IoFilterGroup *filterGroup, PackRead *packRead);

// Read from the session stream
FN_EXTERN size_t storageReadRemoteStreamRead(StorageReadRemoteStream *this, IoFilterGroup *filterGroup, Buffer *buffer);

/***********************************************************************************************************************************
Macros for function logging
***********************************************************************************************************************************/
#define FUNCTION_LOG_STORAGE_READ_REMOTE_TYPE                                                                                      \
    StorageReadRemote *
#define FUNCTION_LOG_STORAGE_READ_REMOTE_FORMAT(value, buffer, bufferSize)                                                         \
    objNameToLog(value, "StorageReadRemote", buffer, bufferSize)

#endif
