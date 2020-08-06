/***********************************************************************************************************************************
Socket Session

A simple socket session intended to allow access to services that are exposed via a socket.
***********************************************************************************************************************************/
#ifndef COMMON_IO_SOCKET_SESSION_H
#define COMMON_IO_SOCKET_SESSION_H

#include "common/io/session.h"
#include "common/time.h"

/***********************************************************************************************************************************
Constructors
***********************************************************************************************************************************/
IoSession *sckSessionNew(IoSessionRole role, int fd, const String *host, unsigned int port, TimeMSec timeout);


#endif
