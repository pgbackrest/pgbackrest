/***********************************************************************************************************************************
Harness for Protocol Testing
***********************************************************************************************************************************/
#include "protocol/server.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
// Install/uninstall the shim that allows protocalLocalGet() to start a local in a forked process rather than being exec'd. The main
// benefit is that code running in the forked process will be included in coverage so no separate tests for the local protocol
// functions should be required. A side benefit is that the pgbackrest binary does not need to be built since there is no exec.
void hrnProtocolLocalShimInstall(const ProtocolServerHandler *const handlerList, const unsigned int handlerListSize);
void hrnProtocolLocalShimUninstall(void);

// Install/uninstall the shim that allows protocalRemoteGet() to start a remote in a forked process rather than being exec'd via
// SSH. The benefits are the same as hrnProtocolLocalShimInstall().
void hrnProtocolRemoteShimInstall(const ProtocolServerHandler *const handlerList, const unsigned int handlerListSize);
void hrnProtocolRemoteShimUninstall(void);
