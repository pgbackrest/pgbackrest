/***********************************************************************************************************************************
PostgreSQL 8.3 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE083_H
#define POSTGRES_INTERFACE_INTERFACE083_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs083(const Buffer *controlFile);
PgControl pgInterfaceControl083(const Buffer *controlFile);
bool pgInterfaceWalIs083(const Buffer *walFile);
PgWal pgInterfaceWal083(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest083(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest083(PgWal pgWal, Buffer *buffer);
#endif

#endif
