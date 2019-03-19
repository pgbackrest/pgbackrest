/***********************************************************************************************************************************
PostgreSQL 9.6 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE096_H
#define POSTGRES_INTERFACE_INTERFACE096_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs096(const Buffer *controlFile);
PgControl pgInterfaceControl096(const Buffer *controlFile);
bool pgInterfaceWalIs096(const Buffer *walFile);
PgWal pgInterfaceWal096(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest096(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest096(PgWal pgWal, Buffer *buffer);
#endif

#endif
