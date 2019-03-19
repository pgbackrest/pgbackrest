/***********************************************************************************************************************************
PostgreSQL 8.4 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE084_H
#define POSTGRES_INTERFACE_INTERFACE084_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs084(const Buffer *controlFile);
PgControl pgInterfaceControl084(const Buffer *controlFile);
bool pgInterfaceWalIs084(const Buffer *walFile);
PgWal pgInterfaceWal084(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest084(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest084(PgWal pgWal, Buffer *buffer);
#endif

#endif
