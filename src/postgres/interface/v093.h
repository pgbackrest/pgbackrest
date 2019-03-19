/***********************************************************************************************************************************
PostgreSQL 9.3 Interface
***********************************************************************************************************************************/
#ifndef POSTGRES_INTERFACE_INTERFACE093_H
#define POSTGRES_INTERFACE_INTERFACE093_H

#include "postgres/interface.h"

/***********************************************************************************************************************************
Functions
***********************************************************************************************************************************/
bool pgInterfaceControlIs093(const Buffer *controlFile);
PgControl pgInterfaceControl093(const Buffer *controlFile);
bool pgInterfaceWalIs093(const Buffer *walFile);
PgWal pgInterfaceWal093(const Buffer *controlFile);

/***********************************************************************************************************************************
Test Functions
***********************************************************************************************************************************/
#ifdef DEBUG
    void pgInterfaceControlTest093(PgControl pgControl, Buffer *buffer);
    void pgInterfaceWalTest093(PgWal pgWal, Buffer *buffer);
#endif

#endif
