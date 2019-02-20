/***********************************************************************************************************************************
Remote Storage Protocol Handler
***********************************************************************************************************************************/
#include "common/debug.h"
#include "common/io/io.h"
#include "common/log.h"
#include "common/memContext.h"
#include "storage/driver/remote/protocol.h"
#include "storage/helper.h"

/***********************************************************************************************************************************
Constants
***********************************************************************************************************************************/
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_EXISTS_STR,                  PROTOCOL_COMMAND_STORAGE_EXISTS);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_LIST_STR,                    PROTOCOL_COMMAND_STORAGE_LIST);
STRING_EXTERN(PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR,               PROTOCOL_COMMAND_STORAGE_OPEN_READ);

/***********************************************************************************************************************************
Process storage protocol requests
***********************************************************************************************************************************/
bool
storageDriverRemoteProtocol(const String *command, const VariantList *paramList, ProtocolServer *server)
{
    FUNCTION_LOG_BEGIN(logLevelTrace);
        FUNCTION_LOG_PARAM(STRING, command);
        FUNCTION_LOG_PARAM(VARIANT_LIST, paramList);
        FUNCTION_LOG_PARAM(PROTOCOL_SERVER, server);
    FUNCTION_LOG_END();

    ASSERT(command != NULL);

    // Determine which storage should be used (??? for now this is only repo)
    const Storage *storage = storageRepo();

    // Attempt to satisfy the request -- we may get requests that are meant for other handlers
    bool found = true;

    MEM_CONTEXT_TEMP_BEGIN()
    {
        if (strEq(command, PROTOCOL_COMMAND_STORAGE_EXISTS_STR))
        {
            protocolServerResponse(server, varNewBool(storageExistsNP(storage, varStr(varLstGet(paramList, 0)))));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_LIST_STR))
        {
            protocolServerResponse(
                server,
                varNewVarLst(
                    varLstNewStrLst(
                        storageListP(
                            storage, varStr(varLstGet(paramList, 0)), .errorOnMissing = varBool(varLstGet(paramList, 1)),
                            varStr(varLstGet(paramList, 2))))));
        }
        else if (strEq(command, PROTOCOL_COMMAND_STORAGE_OPEN_READ_STR))
        {
            // Create the read object
            IoRead *fileRead = storageFileReadIo(
                storageNewReadP(storage, varStr(varLstGet(paramList, 0)), .ignoreMissing = varBool(varLstGet(paramList, 1))));

            // Check if the file exists
            bool exists = ioReadOpen(fileRead);
            protocolServerResponse(server, varNewBool(exists));

            // Transfer the file if it exists
            if (exists)
            {
                Buffer *buffer = bufNew(ioBufferSize());

                // Write file out to protocol layer
                do
                {
                    ioRead(fileRead, buffer);

                    if (bufUsed(buffer) > 0)
                    {
                        ioWriteLine(protocolServerIoWrite(server), strNewFmt(PROTOCOL_BLOCK_HEADER "%zu", bufUsed(buffer)));
                        ioWrite(protocolServerIoWrite(server), buffer);
                        ioWriteFlush(protocolServerIoWrite(server));

                        bufUsedZero(buffer);
                    }
                }
                while (!ioReadEof(fileRead));

                // Write a zero block to show file is complete
                ioWriteLine(protocolServerIoWrite(server), strNew(PROTOCOL_BLOCK_HEADER "0"));
                ioWriteFlush(protocolServerIoWrite(server));
            }
        }
        else
            found = false;
    }
    MEM_CONTEXT_TEMP_END();

    FUNCTION_LOG_RETURN(BOOL, found);
}
